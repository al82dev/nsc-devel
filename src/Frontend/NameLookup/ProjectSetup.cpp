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
std::string pathKey(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
    if (errorCode)
    {
        canonical = std::filesystem::absolute(path).lexically_normal();
    }

    std::string key = canonical.generic_string();
    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return key;
}

std::string sourcePathForUnit(
    const compilation::ProjectGraph& graph,
    const ProjectSyntaxUnit& unit)
{
    const std::span<const compilation::Project> projects = graph.projects();
    if (unit.projectOrdinal() < projects.size())
    {
        for (const compilation::ProjectSource& source :
             projects[unit.projectOrdinal()].sourceSet().sources())
        {
            if (source.ordinal == unit.sourceOrdinal())
            {
                return source.absolutePath.generic_string();
            }
        }
    }
    return "<file-id:" + std::to_string(unit.fileId().value()) + ">";
}
}

namespace
{
/// Lower-cases every byte with std::tolower, as typeIsVisibleTo compared package, module and
/// friend names.
std::string foldedVisibilityName(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
}

void ProjectNameLookup::initializeModules(const compilation::ProjectGraph& graph)
{
    const std::span<const compilation::Project> projects = graph.projects();
    modules_.reserve(projects.size());
    typesByModule_.resize(projects.size());
    nestedSourceNamesByModule_.resize(projects.size());
    reexportedTypesByModule_.resize(projects.size());
    packageByProject_.resize(projects.size());
    friendsByProject_.resize(projects.size());
    foldedPackageByProject_.resize(projects.size());
    foldedModuleByProject_.resize(projects.size());
    foldedFriendsByProject_.resize(projects.size());
    for (std::size_t projectOrdinal = 0; projectOrdinal < projects.size(); ++projectOrdinal)
    {
        modules_.push_back(std::make_unique<ModuleDeclarationSpace>(
            projectOrdinal,
            projects[projectOrdinal].moduleName()));
        packageByProject_[projectOrdinal] = std::string(projects[projectOrdinal].packageName());
        foldedPackageByProject_[projectOrdinal] = foldedVisibilityName(packageByProject_[projectOrdinal]);
        friendsByProject_[projectOrdinal].assign(
            projects[projectOrdinal].friendModules().begin(),
            projects[projectOrdinal].friendModules().end());
        foldedModuleByProject_[projectOrdinal] = foldedVisibilityName(modules_.back()->moduleName());
        for (const std::string& friendModule : friendsByProject_[projectOrdinal])
            foldedFriendsByProject_[projectOrdinal].insert(foldedVisibilityName(friendModule));
    }
}

void ProjectNameLookup::collectDeclarationIndexes(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    driver::CompilationWorkerPool& workers)
{
    std::vector<UnitDeclarationIndex> declarationIndexes(units.size());
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        if (units[index] == nullptr || units[index]->syntaxTree() == nullptr) continue;
        workers.submit([&, index](std::size_t) {
            indexUnitDeclarations(
                *units[index], sourcePathForUnit(graph, *units[index]), declarationIndexes[index]);
        });
    }
    workers.wait();
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        if (units[index] == nullptr) continue;
        UnitDeclarationIndex& unitIndex = declarationIndexes[index];
        for (const std::vector<std::string>& namespacePath : unitIndex.namespaces)
        {
            modules_[units[index]->projectOrdinal()]->globalNamespace().ensureNamespace(namespacePath);
        }
        pendingTypes_.insert(
            pendingTypes_.end(),
            std::make_move_iterator(unitIndex.pendingTypes.begin()),
            std::make_move_iterator(unitIndex.pendingTypes.end()));
        namespaceFunctions_.insert(
            namespaceFunctions_.end(),
            std::make_move_iterator(unitIndex.namespaceFunctions.begin()),
            std::make_move_iterator(unitIndex.namespaceFunctions.end()));
        namespaceStates_.insert(
            namespaceStates_.end(),
            std::make_move_iterator(unitIndex.namespaceStates.begin()),
            std::make_move_iterator(unitIndex.namespaceStates.end()));
    }
}

bool ProjectNameLookup::namespaceFunctionLess(
    const NamespaceFunctionDeclaration& left,
    const NamespaceFunctionDeclaration& right)
{
    if (left.moduleName != right.moduleName) return left.moduleName < right.moduleName;
    if (left.namespaceName != right.namespaceName) return left.namespaceName < right.namespaceName;
    if (left.name != right.name) return left.name < right.name;
    if (left.sourceOrdinal != right.sourceOrdinal) return left.sourceOrdinal < right.sourceOrdinal;
    return left.range.begin().offset() < right.range.begin().offset();
}

bool ProjectNameLookup::namespaceStateLess(
    const NamespaceStateDeclaration& left,
    const NamespaceStateDeclaration& right)
{
    if (left.moduleName != right.moduleName) return left.moduleName < right.moduleName;
    if (left.namespaceName != right.namespaceName) return left.namespaceName < right.namespaceName;
    if (left.name != right.name) return left.name < right.name;
    if (left.sourceOrdinal != right.sourceOrdinal) return left.sourceOrdinal < right.sourceOrdinal;
    return left.range.begin().offset() < right.range.begin().offset();
}

void ProjectNameLookup::mergeBindingIndexes(std::vector<UnitBindingIndex>& bindingIndexes)
{
    for (UnitBindingIndex& unitIndex : bindingIndexes)
    {
        const std::size_t bindingBase = bindings_.size();
        bindings_.insert(
            bindings_.end(),
            std::make_move_iterator(unitIndex.bindings.begin()),
            std::make_move_iterator(unitIndex.bindings.end()));
        for (const auto& [syntax, ordinal] : unitIndex.bindingBySyntax)
            bindingBySyntax_.emplace(syntax, bindingBase + ordinal);
        const std::size_t expressionBase = expressionTypeBindings_.size();
        expressionTypeBindings_.insert(
            expressionTypeBindings_.end(),
            std::make_move_iterator(unitIndex.expressionTypeBindings.begin()),
            std::make_move_iterator(unitIndex.expressionTypeBindings.end()));
        for (const auto& [syntax, ordinal] : unitIndex.expressionTypeBindingBySyntax)
            expressionTypeBindingBySyntax_.emplace(syntax, expressionBase + ordinal);
    }
}

void ProjectNameLookup::collectBindingIndexes(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics,
    driver::CompilationWorkerPool& workers)
{
    std::vector<UnitBindingIndex> bindingIndexes(units.size());
    std::vector<diagnostics::DiagnosticEngine> localDiagnostics;
    localDiagnostics.reserve(units.size());
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        localDiagnostics.emplace_back(diagnostics.errorLimit());
        if (units[index] == nullptr || units[index]->syntaxTree() == nullptr) continue;
        // EN: A restored API unit with an accepted snapshot takes its bindings from it (plan 3.1).
        // ES: Una unidad de API restaurada con snapshot aceptado toma de él sus bindings (plan 3.1).
        const PrecompiledSnapshotUnit* const snapshot = snapshotForUnit(index);
        if (snapshot != nullptr && installUnitBindings(*units[index], *snapshot, bindingIndexes[index]))
        {
            ++installedSnapshotUnits_;
            continue;
        }
        workers.submit([&, index](std::size_t) {
            bindUnit(*units[index], localDiagnostics[index], bindingIndexes[index]);
        });
    }
    workers.wait();
    mergeBindingIndexes(bindingIndexes);
    diagnostics.mergeDeterministically(localDiagnostics);
}

void ProjectNameLookup::buildVisibility(const compilation::ProjectGraph& graph)
{
    const std::span<const compilation::Project> projects = graph.projects();
    std::unordered_map<std::string, std::size_t> projectByPath;
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        projectByPath.emplace(pathKey(projects[ordinal].projectFile()), ordinal);
    }

    // Every reference is canonicalized once; the closure of each project then walks ordinals only.
    // pathKey touches the file system, and walking it inside the closure repeated that work once
    // per project and per edge.
    std::vector<std::vector<std::size_t>> dependencies(projects.size());
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        for (const std::filesystem::path& reference : projects[ordinal].projectReferences())
        {
            const auto iterator = projectByPath.find(pathKey(reference));
            if (iterator != projectByPath.end())
            {
                dependencies[ordinal].push_back(iterator->second);
            }
        }
    }

    for (std::size_t start = 0; start < projects.size(); ++start)
    {
        std::vector<bool> reachable(projects.size(), false);
        std::vector<std::size_t> pending = {start};
        while (!pending.empty())
        {
            const std::size_t ordinal = pending.back();
            pending.pop_back();
            for (const std::size_t dependency : dependencies[ordinal])
            {
                if (reachable[dependency])
                {
                    continue;
                }
                reachable[dependency] = true;
                pending.push_back(dependency);
            }
        }

        std::vector<std::size_t> visible;
        for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
        {
            if (reachable[ordinal])
            {
                visible.push_back(ordinal);
            }
        }
        modules_[start]->setVisibleProjectOrdinals(std::move(visible));
    }
}

void ProjectNameLookup::indexUnitDeclarations(
    const ProjectSyntaxUnit& unit,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    predeclareNode(unit, unit.syntaxTree(), {}, {}, sourcePath, index);
}

}
