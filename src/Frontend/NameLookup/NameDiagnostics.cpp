#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../Internal/LookupEnvironment.h"
#include "../Internal/ProjectNameLookupFrame.h"

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
std::string candidateList(
    std::span<const std::size_t> candidates,
    std::span<const NamedTypeDeclaration> declarations)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (index != 0)
        {
            output << ", ";
        }
        output << declarations[candidates[index]].identity.key();
    }
    return output.str();
}
}

void ProjectNameLookup::bindUsingAliasDirective(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* directive,
    const std::vector<std::string>& namespaceSegments,
    LookupEnvironment& environment,
    std::unordered_set<std::string>& localAliases,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!localAliases.insert(directive->text()).second)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateUsingAlias,
            diagnostics::DiagnosticSeverity::Error,
            directive->range(),
            "duplicate using alias '" + directive->text() + "' in the same declaration scope");
        return;
    }
    const std::optional<std::string> namespaceTarget = directive->auxiliaryText().empty()
        ? std::nullopt
        : resolveNamespaceTarget(
            unit.projectOrdinal(), namespaceSegments, directive->auxiliaryText());
    // C3: a target with modifiers (`void*`, `float[4]`) or a builtin base (`uint`) is a
    // TYPE by construction -- there is nothing to look up and nothing that could be
    // ambiguous, so the name-resolution checks below apply only to a bare name target.
    const bool targetIsWrittenAsType = directive->auxiliaryText().empty();
    // C3 "alias chaining": `using LPDWORD = PDWORD;` names another ALIAS, not a type, so
    // the type-visibility check below would reject it. An alias already in scope is a
    // legitimate target -- expansion follows the chain at bind time -- and one that is
    // NOT in scope still falls through to the real diagnostic.
    const bool targetIsAliasInScope = !directive->auxiliaryText().empty() &&
        (environment.aliases.contains(directive->auxiliaryText()) ||
         environment.typeAliases.contains(directive->auxiliaryText()));
    if (!namespaceTarget.has_value() && !targetIsWrittenAsType && !targetIsAliasInScope)
    {
        const std::vector<std::size_t> typeCandidates = resolveTypeCandidates(
            unit.projectOrdinal(), namespaceSegments, environment, directive->auxiliaryText());
        if (typeCandidates.empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidUsingTarget,
                diagnostics::DiagnosticSeverity::Error,
                directive->range(),
                "using alias target '" + directive->auxiliaryText() +
                    "' does not name a visible namespace or type");
            return;
        }
        if (typeCandidates.size() > 1)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::AmbiguousTypeName,
                diagnostics::DiagnosticSeverity::Error,
                directive->range(),
                "using alias target '" + directive->auxiliaryText() +
                    "' is ambiguous; candidates: " + candidateList(typeCandidates, namedTypes_));
            return;
        }
    }
    // C3 "FFI aliases": a NAMESPACE alias keeps the textual path (`aliases`); a TYPE
    // alias keeps its parsed type syntax (`typeAliases`), which is what lets the target
    // be `uint`, `void*` or `float[4]` and not only a name. resolveNamespaceTarget above
    // is what separates the two, so `using IO = NativeSharp.Core.IO;` is unaffected.
    if (namespaceTarget.has_value() || directive->typeSyntax() == nullptr ||
        environment.aliases.contains(directive->auxiliaryText()))
    {
        environment.aliases[directive->text()] = directive->auxiliaryText();
    }
    else
    {
        environment.typeAliases[directive->text()] = directive->typeSyntax();
    }
}

void ProjectNameLookup::resolveTypeNameBinding(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    TypeNameBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    binding.candidateTypeOrdinals = resolveTypeCandidates(
        binding.projectOrdinal, namespaceSegments, environment, syntax->baseName());
    appendObjectTypeRoot(binding, syntax);
    if (binding.candidateTypeOrdinals.empty())
    {
        binding.kind = TypeNameBindingKind::Unresolved;
        reportUnresolvedType(
            unit, syntax, namespaceSegments, binding.projectOrdinal, diagnostics);
        return;
    }
    if (binding.candidateTypeOrdinals.size() == 1)
    {
        binding.kind = TypeNameBindingKind::Resolved;
        binding.targetTypeOrdinal = binding.candidateTypeOrdinals.front();
        if (syntax->isDynamicTrait() &&
            namedTypes_[*binding.targetTypeOrdinal].kind != NamedTypeDeclarationKind::Interface)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                syntax->range(),
                "'dyn " + syntax->baseName() +
                    "' is invalid because the target is not a trait or interface");
        }
        return;
    }
    binding.kind = TypeNameBindingKind::Ambiguous;
    diagnostics.report(
        diagnostics::DiagnosticId::AmbiguousTypeName,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        "ambiguous type name '" + syntax->baseName() + "'; candidates: " +
            candidateList(binding.candidateTypeOrdinals, namedTypes_));
}

void ProjectNameLookup::reportUnsupportedGlobalDeclarations(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit == nullptr || unit->isPrecompiled() || unit->syntaxTree() == nullptr) continue;
        // EN: Only namespace scopes are walked; a class keeps its own `extern C from` blocks.
        // ES: Solo se recorren ambitos de namespace; una clase conserva sus bloques `extern C from`.
        std::vector<std::pair<const ast::AstNode*, bool>> pending = {{unit->syntaxTree(), true}};
        while (!pending.empty())
        {
            const auto [scope, globalNamespace] = pending.back();
            pending.pop_back();
            for (const ast::AstNode* child : scope->children())
            {
                if (child == nullptr) continue;
                if (child->kind() == ast::AstNodeKind::NamespaceDeclaration)
                    pending.push_back({child, false});
                else if (child->kind() == ast::AstNodeKind::ExternCDeclaration)
                    diagnostics.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        child->range(),
                        "an 'extern C' import outside a class is not supported in NativeSharp 3.0; "
                        "declare it in a class: extern C from \"<library>.dll\" { ... }");
                else if (globalNamespace && child->kind() == ast::AstNodeKind::GlobalVariableDeclaration)
                    diagnostics.report(
                        diagnostics::DiagnosticId::ExpectedDeclaration,
                        diagnostics::DiagnosticSeverity::Error,
                        child->range(),
                        "a variable outside every namespace is not supported in NativeSharp 3.0; "
                        "declare it inside a namespace or as a static field");
            }
        }
    }
}

}
