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
void ProjectNameLookup::collectReexports(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::string& namespaceName,
    std::vector<PendingReexport>& pending) const
{
    if (node == nullptr) return;
    std::string childNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        if (!childNamespace.empty()) childNamespace.push_back('.');
        childNamespace += node->text();
    }
    if (node->flag() && node->kind() == ast::AstNodeKind::UsingAliasDirective)
    {
        std::string publicName = namespaceName;
        if (!publicName.empty()) publicName.push_back('.');
        publicName += node->text();
        pending.push_back(PendingReexport{
            unit.projectOrdinal(), std::move(publicName), node->auxiliaryText(), node->range()});
    }
    else if (node->flag() && node->kind() == ast::AstNodeKind::UsingNamespaceDirective)
    {
        const std::string prefix = node->text() + ".";
        if (unit.projectOrdinal() < modules_.size())
        {
            for (const std::size_t visible :
                 modules_[unit.projectOrdinal()]->visibleProjectOrdinals())
            {
                if (visible >= typesByModule_.size()) continue;
                for (const auto& [name, ordinal] : typesByModule_[visible])
                {
                    static_cast<void>(ordinal);
                    if (name.starts_with(prefix))
                        pending.push_back(PendingReexport{
                            unit.projectOrdinal(), name, name, node->range()});
                }
            }
        }
    }
    for (const ast::AstNode* child : node->children())
        collectReexports(unit, child, childNamespace, pending);
}

void ProjectNameLookup::appendReexportCandidate(
    std::vector<std::size_t>& candidates,
    std::size_t moduleOrdinal,
    const std::string& targetName) const
{
    const std::optional<std::size_t> found = findTypeInModule(moduleOrdinal, targetName);
    if (found.has_value() &&
        std::find(candidates.begin(), candidates.end(), *found) == candidates.end())
        candidates.push_back(*found);
}

bool ProjectNameLookup::isReexportPubliclyExportable(std::size_t target) const
{
    std::optional<std::size_t> current = target;
    while (current.has_value() && *current < namedTypes_.size())
    {
        if (namedTypes_[*current].accessibility != NamedTypeAccessibility::Public)
        {
            return false;
        }
        current = namedTypes_[*current].containingTypeOrdinal;
    }
    return true;
}

void ProjectNameLookup::finalizeReexport(
    const PendingReexport& item,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (item.ownerProjectOrdinal >= modules_.size()) return;
    std::vector<std::size_t> candidates;
    appendReexportCandidate(candidates, item.ownerProjectOrdinal, item.targetName);
    for (const std::size_t visible : modules_[item.ownerProjectOrdinal]->visibleProjectOrdinals())
        appendReexportCandidate(candidates, visible, item.targetName);
    if (candidates.size() != 1U)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownTypeName,
            diagnostics::DiagnosticSeverity::Error,
            item.range,
            candidates.empty()
                ? "re-export target '" + item.targetName + "' does not name a visible type"
                : "re-export target '" + item.targetName + "' is ambiguous");
        return;
    }
    const std::size_t target = candidates.front();
    if (!isReexportPubliclyExportable(target))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownTypeName,
            diagnostics::DiagnosticSeverity::Error,
            item.range,
            "re-export target '" + item.targetName + "' is not public");
        return;
    }
    if (typesByModule_[item.ownerProjectOrdinal].contains(item.publicName) ||
        nestedSourceNamesByModule_[item.ownerProjectOrdinal].contains(item.publicName))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            item.range,
            "re-export name '" + item.publicName + "' conflicts with a declaration");
        return;
    }
    auto& table = reexportedTypesByModule_[item.ownerProjectOrdinal];
    const auto [entry, inserted] = table.emplace(item.publicName, target);
    if (!inserted && entry->second != target)
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            item.range,
            "re-export name '" + item.publicName + "' resolves to multiple identities");
}

bool ProjectNameLookup::pendingReexportLess(
    const PendingReexport& left,
    const PendingReexport& right)
{
    return std::tie(left.ownerProjectOrdinal, left.publicName, left.targetName) <
        std::tie(right.ownerProjectOrdinal, right.publicName, right.targetName);
}

void ProjectNameLookup::finalizeReexports(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<PendingReexport> pending;
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
        if (unit != nullptr)
            collectReexports(*unit, unit->syntaxTree(), std::string{}, pending);

    std::ranges::sort(pending, pendingReexportLess);
    for (const PendingReexport& item : pending) finalizeReexport(item, diagnostics);
}

}
