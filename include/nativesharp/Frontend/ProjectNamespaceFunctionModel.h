#pragma once

#include "nativesharp/Basic/TransparentStringHash.h"
#include "nativesharp/Frontend/NamespaceFunctionPart.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/PrecompiledNamespaceFunctionIdentity.h"
#include "nativesharp/Frontend/NamespaceStateInitializerKind.h"
#include "nativesharp/Frontend/NamespaceStateSymbol.h"
#include "nativesharp/Frontend/NamespaceFunctionReferenceBinding.h"
#include "nativesharp/Frontend/NamespaceStateReferenceBinding.h"
#include "nativesharp/Frontend/NamespaceCompletionScope.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class ProjectGenericModel;
namespace internal
{
struct FunctionLookupEnvironment;
struct WalkWork;
}







class ProjectNamespaceFunctionModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericModel* genericModel = nullptr);
    bool bindPrecompiledEscapeSummaries(
        std::span<const PrecompiledNamespaceFunctionIdentity> identities,
        std::string& error);

    std::span<const NamespaceFunctionSymbol> functions() const noexcept;
    std::span<const NamespaceStateSymbol> states() const noexcept;
    const NamespaceStateSymbol* findState(
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view name) const noexcept;
    std::span<const std::size_t> findFunctions(
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view name) const noexcept;
    std::span<const std::size_t> extensionFunctionsNamed(
        std::string_view name) const noexcept;
    bool hasExtensionFunctionIndex() const noexcept;
    const NamespaceFunctionReferenceBinding* referenceBinding(
        const ast::AstNode* callSyntax) const noexcept;
    const NamespaceFunctionReferenceBinding* extensionBinding(
        const ast::AstNode* memberSyntax) const noexcept;
    std::span<const NamespaceFunctionReferenceBinding> referenceBindings() const noexcept;
    const NamespaceStateReferenceBinding* stateReferenceBinding(
        const ast::AstNode* expressionSyntax) const noexcept;
    std::span<const NamespaceStateReferenceBinding> stateReferenceBindings() const noexcept;
    std::span<const NamespaceCompletionScope> completionScopes() const noexcept;
    bool isBuilt() const noexcept;

    // EN: Plan item 3.4. Accepted snapshots of restored public API units, set before build(). A
    //     namespace function or state whose declaration node has an entry in the `namespace-members`
    //     section takes its symbol from it; the declaration-copied fields, merging, duplicate checks
    //     and every index are still computed exactly as for a computed symbol.
    // ES: Punto 3.4 del plan. Snapshots aceptados de unidades de API pública restauradas, asignados
    //     antes de build(). Una función o un estado de namespace cuyo nodo de declaración tiene
    //     entrada en la sección `namespace-members` toma de ella su símbolo; los campos copiados de
    //     la declaración, la fusión, las comprobaciones de duplicados y todos los índices se siguen
    //     calculando igual que para un símbolo calculado.
    void setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots);
    std::size_t installedSnapshotFunctionCount() const noexcept;
    std::size_t installedSnapshotStateCount() const noexcept;
    // EN: Producer side. The reference compilation names the FileId of its captured API unit before
    //     build(); every function and state symbol computed from a declaration of that file is kept
    //     and written by writePrecompiledSnapshotNamespaceMembers.
    // ES: Lado productor. La compilación de referencia nombra el FileId de su unidad de API capturada
    //     antes de build(); cada símbolo de función y de estado calculado desde una declaración de
    //     ese archivo se conserva y lo escribe writePrecompiledSnapshotNamespaceMembers.
    void setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept;
    bool writePrecompiledSnapshotNamespaceMembers(
        const PrecompiledSyntaxNodeIndex& index,
        std::string& payload,
        std::string& error) const;

private:
    // EN: namespace -> name -> ordinals of every project, filled next to functionsByName_ and
    //     stateByName_. A reference resolves with one lookup instead of one per visible project.
    // ES: namespace -> nombre -> ordinales de todos los proyectos, llenado junto a
    //     functionsByName_ y stateByName_. Una referencia se resuelve con una búsqueda en lugar de
    //     una por proyecto visible.
    using OrdinalsByName = std::unordered_map<
        std::string, std::vector<std::size_t>, basic::TransparentStringHash, std::equal_to<>>;
    using OrdinalsByNamespaceName = std::unordered_map<
        std::string, OrdinalsByName, basic::TransparentStringHash, std::equal_to<>>;

    static std::span<const std::size_t> ordinalsInNamespace(
        const OrdinalsByNamespaceName& index,
        std::string_view namespaceName,
        std::string_view name) noexcept;
    static bool symbolVisibleFrom(
        std::size_t symbolProject,
        NamespaceFunctionAccessibility accessibility,
        std::size_t projectOrdinal,
        std::span<const std::size_t> visible) noexcept;
    void resetBuildState();
    void buildExtensionFunctionIndex();
    void collectNamespaceFunctions(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericModel* genericModel,
        std::unordered_map<std::string, std::size_t>& symbolByIdentity,
        std::unordered_map<std::string, std::size_t>& exportedCByName);
    void collectNamespaceFunction(
        const NamespaceFunctionDeclaration& declaration,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericModel* genericModel,
        std::unordered_map<std::string, std::size_t>& symbolByIdentity,
        std::unordered_map<std::string, std::size_t>& exportedCByName);
    void initializeNamespaceFunction(
        NamespaceFunctionSymbol& candidate,
        const NamespaceFunctionDeclaration& declaration,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void configureNamespaceFunctionOperator(NamespaceFunctionSymbol& candidate) const;
    void collectNamespaceFunctionParameters(
        NamespaceFunctionSymbol& candidate,
        const NamespaceFunctionDeclaration& declaration,
        const ast::AstNode& syntax,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericModel* genericModel) const;
    MethodParameterSymbol initializeNamespaceFunctionParameter(
        const ast::AstNode& parameterNode,
        const NamespaceFunctionDeclaration& declaration,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel) const;
    void validateNamespaceFunctionParameterPack(
        MethodParameterSymbol& parameter,
        bool& sawParameterPack,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericModel* genericModel) const;
    void bindNamespaceFunctionParameterLifetime(
        MethodParameterSymbol& parameter,
        const ast::AstNode& syntax,
        std::size_t parameterIndex,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordNamespaceFunctionParameter(
        NamespaceFunctionSymbol& candidate,
        MethodParameterSymbol parameter,
        const NamespaceFunctionDeclaration& declaration,
        std::unordered_map<std::string, const ast::AstNode*>& parameterNames,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateNamespaceFunctionLifetimes(
        NamespaceFunctionSymbol& candidate,
        const ast::AstNode& syntax,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateNamespaceFunctionReturnLifetimeSource(
        const NamespaceFunctionSymbol& candidate,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void finalizeNamespaceFunctionIdentity(
        NamespaceFunctionSymbol& candidate,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void mergeNamespaceFunction(
        NamespaceFunctionSymbol candidate,
        const NamespaceFunctionDeclaration& declaration,
        diagnostics::DiagnosticEngine& diagnostics,
        std::unordered_map<std::string, std::size_t>& symbolByIdentity,
        std::unordered_map<std::string, std::size_t>& exportedCByName);
    bool installSnapshotFunction(
        const NamespaceFunctionDeclaration& declaration,
        NamespaceFunctionSymbol& candidate);
    void captureSnapshotFunction(
        const NamespaceFunctionDeclaration& declaration,
        const NamespaceFunctionSymbol& candidate);
    bool installSnapshotState(
        const NamespaceStateDeclaration& declaration,
        NamespaceStateSymbol& state);
    void captureSnapshotState(
        const NamespaceStateDeclaration& declaration,
        const NamespaceStateSymbol& state);
    void initializeNamespaceState(
        NamespaceStateSymbol& state,
        const NamespaceStateDeclaration& declaration,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel) const;
    void collectNamespaceStates(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics);
    void collectNamespaceState(
        const NamespaceStateDeclaration& declaration,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics);
    std::span<const std::size_t> visibleProjects(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal) const;
    bool namespaceExists(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal,
        std::string_view namespaceName) const;
    std::optional<std::string> resolveNamespace(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment,
        std::string written) const;
    void appendFunctionCandidates(
        std::vector<std::size_t>& candidates,
        std::size_t projectOrdinal,
        std::size_t visibleProject,
        std::string_view namespaceName,
        std::string_view functionName) const;
    std::vector<std::size_t> candidatesInNamespace(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view functionName) const;
    std::vector<std::size_t> extensionCandidatesInNamespace(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view functionName) const;
    void appendStateCandidate(
        std::vector<std::size_t>& candidates,
        std::size_t projectOrdinal,
        std::size_t visibleProject,
        std::string_view namespaceName,
        std::string_view stateName) const;
    std::vector<std::size_t> stateCandidatesInNamespace(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        std::string_view stateName) const;
    void bindNamespaceReferences(
        const ProjectNameLookup& nameLookup,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units);
    void bindNamespaceReferencesInUnit(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit);
    void walkNamespaceScope(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* scopeNode,
        std::size_t namespaceIndex,
        std::size_t inheritedEnvironmentIndex,
        std::vector<std::string>& lexicalNamespaces,
        std::vector<internal::FunctionLookupEnvironment>& environments,
        std::vector<internal::WalkWork>& work);
    void walkNamespaceNode(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        std::size_t namespaceIndex,
        std::size_t environmentIndex,
        std::vector<std::string>& lexicalNamespaces,
        std::vector<internal::FunctionLookupEnvironment>& environments,
        std::vector<internal::WalkWork>& work,
        std::unordered_set<const ast::AstNode*>& visited,
        std::unordered_set<const ast::AstNode*>& directCallCallees);
    bool scheduleNamespaceNodeScope(
        const ast::AstNode* node,
        std::size_t namespaceIndex,
        std::size_t environmentIndex,
        std::vector<std::string>& lexicalNamespaces,
        std::vector<internal::WalkWork>& work) const;
    void bindExtensionReference(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment);
    void bindCallReference(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment,
        std::unordered_set<const ast::AstNode*>& directCallCallees);
    void bindValueReferences(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment,
        const std::unordered_set<const ast::AstNode*>& directCallCallees);
    void bindFunctionValueReference(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment,
        std::string_view written,
        const std::unordered_set<const ast::AstNode*>& directCallCallees);
    void bindStateReference(
        const ProjectNameLookup& nameLookup,
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment,
        std::string_view written);
    std::vector<std::size_t> resolveFunctionCandidates(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment,
        std::string_view written) const;
    std::vector<std::size_t> resolveStateCandidates(
        const ProjectNameLookup& nameLookup,
        std::size_t projectOrdinal,
        std::string_view lexicalNamespace,
        const internal::FunctionLookupEnvironment& environment,
        std::string_view written) const;
    void scheduleNamespaceNodeChildren(
        const ast::AstNode* node,
        std::size_t namespaceIndex,
        std::size_t environmentIndex,
        std::vector<internal::WalkWork>& work) const;

    std::vector<NamespaceFunctionSymbol> functions_;
    std::vector<NamespaceStateSymbol> states_;
    std::unordered_map<std::string, std::size_t> stateByName_;
    std::unordered_map<std::string, std::vector<std::size_t>> functionsByName_;
    OrdinalsByNamespaceName functionsByNamespaceName_;
    OrdinalsByNamespaceName statesByNamespaceName_;
    std::map<std::string, std::vector<std::size_t>, std::less<>> extensionFunctionsByName_;
    bool extensionFunctionIndexBuilt_ = false;
    std::vector<NamespaceFunctionReferenceBinding> referenceBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> referenceBindingBySyntax_;
    std::vector<NamespaceFunctionReferenceBinding> extensionBindings_;
    // NS304-T08-F06: indexed by source identity (file + begin offset), not by AST node pointer, so
    // the incremental binding survives re-parses that replace the node allocation.
    std::unordered_map<std::uint64_t, std::size_t> extensionBindingBySyntax_;
    std::vector<NamespaceStateReferenceBinding> stateReferenceBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> stateReferenceBindingBySyntax_;
    std::vector<NamespaceCompletionScope> completionScopes_;
    std::unordered_map<const ast::AstNode*, NamespaceFunctionSymbol> snapshotFunctions_;
    std::unordered_map<const ast::AstNode*, NamespaceStateSymbol> snapshotStates_;
    std::size_t installedSnapshotFunctions_ = 0;
    std::size_t installedSnapshotStates_ = 0;
    std::optional<FileId> snapshotCaptureFile_;
    std::vector<std::pair<const ast::AstNode*, NamespaceFunctionSymbol>> capturedFunctions_;
    std::vector<std::pair<const ast::AstNode*, NamespaceStateSymbol>> capturedStates_;
    bool built_ = false;
};
}
