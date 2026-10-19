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
void ProjectNameLookup::findReferencedInaccessibleType(
    std::size_t projectOrdinal,
    const std::string& wanted,
    std::string& internalMatch,
    std::optional<std::size_t>& inaccessibleOrdinal) const
{
    if (projectOrdinal >= modules_.size() || modules_[projectOrdinal] == nullptr) return;
    const std::string suffix = "." + wanted;
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        if (visibleOrdinal >= typesByModule_.size()) continue;
        for (const auto& entry : typesByModule_[visibleOrdinal])
        {
            if (entry.second >= namedTypes_.size()) continue;
            if (typeIsVisibleTo(projectOrdinal, entry.second)) continue;
            if (entry.first != wanted &&
                (entry.first.size() <= suffix.size() ||
                 entry.first.compare(
                     entry.first.size() - suffix.size(), suffix.size(), suffix) != 0))
                continue;
            internalMatch = entry.first;
            inaccessibleOrdinal = entry.second;
            break;
        }
        if (!internalMatch.empty()) break;
    }
}

void ProjectNameLookup::reportInaccessibleNestedType(
    const ast::TypeSyntax* syntax,
    std::size_t inaccessibleOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const NamedTypeDeclaration& nested = namedTypes_[inaccessibleOrdinal];
    diagnostics.report(
        diagnostics::DiagnosticId::UnknownTypeName,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        "nested type '" + nested.identity.fullyQualifiedName() + "' is " +
            namedTypeAccessibilityName(nested.accessibility) +
            " and is not accessible here; it is visible only inside '" +
            (nested.containingTypeOrdinal.has_value()
                 ? namedTypes_[*nested.containingTypeOrdinal].identity.fullyQualifiedName()
                 : std::string("its container")) +
            "'");
}

void ProjectNameLookup::reportInaccessibleReferencedType(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::string& internalMatch,
    std::optional<std::size_t> inaccessibleOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const bool packageMismatch = inaccessibleOrdinal.has_value() &&
        namedTypes_[*inaccessibleOrdinal].accessibility == NamedTypeAccessibility::Package;
    diagnostics.report(
        diagnostics::DiagnosticId::UnknownTypeName,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        packageMismatch
            ? "type '" + internalMatch + "' is package-visible and the consuming "
              "project has a different Package identity (module '" +
                  std::string(unit.moduleName()) + "')"
            : "type '" + internalMatch + "' is declared in a referenced project but is "
              "module-visible and the consumer is not an explicit Friend; declare it "
              "'public' or add a Friend contract (module '" +
                  std::string(unit.moduleName()) + "')");
}

void ProjectNameLookup::reportUnresolvedType(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments,
    std::size_t projectOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // Before saying the name is unknown, check whether it EXISTS in a referenced project
    // and is simply not public. Reporting "unknown type name" for a type that is right
    // there sends the reader hunting for a typo that does not exist.
    //
    // This is half of what made the "an extern C delegate does not cross a
    // ProjectReference" report so hard to read. The generated binding declared
    //
    //     extern C delegate long WNDPROC(nint, uint, ulong, long);
    //
    // with no accessibility modifier -- so, internal -- while the classes and structs
    // beside it were written `public` and crossed the boundary without trouble. Only
    // public types cross a module boundary, which is the same rule as C# and is right;
    // what was wrong was that the parser had no branch letting an `extern C delegate`
    // carry an accessibility modifier at all, so it could not be made public even if you
    // tried. That half is fixed in Parser.cpp. This half is the diagnostic, which said
    // "unknown type name" and sent the reader looking for a typo instead of at the
    // declaration that was sitting right there.
    // NSC-LIMIT-009. Antes que nada: el nombre puede estar nombrando un anidado que SI
    // existe y que este sitio no puede ver. Es la misma clase de error que el de abajo
    // --el tipo esta escrito, la accesibilidad es lo que falla-- y merece el mismo trato.
    const std::optional<std::size_t> inaccessibleNested =
        findInaccessibleNestedType(projectOrdinal, namespaceSegments, syntax->baseName());
    if (inaccessibleNested.has_value())
    {
        reportInaccessibleNestedType(syntax, *inaccessibleNested, diagnostics);
        return;
    }
    std::string internalMatch;
    std::optional<std::size_t> inaccessibleOrdinal;
    findReferencedInaccessibleType(
        projectOrdinal, syntax->baseName(), internalMatch, inaccessibleOrdinal);
    if (!internalMatch.empty())
    {
        reportInaccessibleReferencedType(
            unit, syntax, internalMatch, inaccessibleOrdinal, diagnostics);
        return;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::UnknownTypeName,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        "unknown type name '" + syntax->baseName() + "' in module '" +
            std::string(unit.moduleName()) + "'");
}

}
