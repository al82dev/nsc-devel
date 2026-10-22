#pragma once

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nativesharp/Frontend/ProjectOopMirAbiPassing.h"
#include "nativesharp/Frontend/ProjectOopMirNodeKind.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"
#include "nativesharp/Frontend/ProjectOopMirFieldInitializer.h"
#include "nativesharp/Frontend/ProjectOopMirStructLayout.h"
#include "nativesharp/Frontend/ProjectOopMirClassLayout.h"
#include "nativesharp/Frontend/ProjectOopMirFunctionAbi.h"
#include "nativesharp/Frontend/ProjectOopMirCleanupKind.h"
#include "nativesharp/Frontend/ProjectOopMirCleanupRegion.h"
#include "nativesharp/Frontend/ProjectOopMirNonlocalExitKind.h"
#include "nativesharp/Frontend/ProjectOopMirNonlocalExit.h"
#include "nativesharp/Frontend/ProjectOopMirExceptionFlow.h"
#include "nativesharp/Frontend/ProjectOopMirStaticField.h"
#include "nativesharp/Frontend/ProjectOopMirDispatchTable.h"
#include "nativesharp/Frontend/ProjectOopMirFunction.h"
#include "nativesharp/Frontend/ProjectOopMirGenericSpecialization.h"
#include "nativesharp/Frontend/ProjectOopMirInterfaceBinding.h"

namespace nativesharp::frontend
{
// The MIR schema is an internal producer/consumer boundary. It is versioned
// independently from the language and runtime ABI so tools can reject a dump
// whose node/ownership semantics they do not understand.
inline constexpr std::uint16_t kProjectOopMirSchemaMajor = 1;
// MINOR 14: namespace state has explicit load/address nodes and stable storage identity.
// MINOR 15: typed params and generalized aggregate spreads have explicit lowering nodes.
// MINOR 16: null-coalescing value/store control flow is explicit and single-evaluation.
// MINOR 17: tuple layouts plus alias-safe deconstruction stores are explicit.
// MINOR 18: abort-profile panic is an explicit divergent node with builtin:never type.
// MINOR 19: closure capture provenance and shared-cell environment layout are inspectable.
// MINOR 20: Optional<T> coalescing preserves its public HasValue/UncheckedValue protocol calls.
// MINOR 21: rectangular native arrays preserve rank and use explicit allocate/index operations.
// MINOR 22: value layouts publish producer-owned, versioned drop-glue identities.
// MINOR 23: Result propagation publishes both carrier protocols, error conversion, and cleanup.
// 1.24 publishes each callable's typed-throws contract independently from native EH regions.
// 1.25 publishes source-ordered catch patterns, their declared-error matches and exhaustiveness.
// 1.26 publishes value-producing exhaustive match control flow and arm cleanup boundaries.
// 1.27 publishes value-producing loop control flow, labelled exits and converted break values.
// 1.28 publishes explicit assert, unreachable, assume and project-wide panic-profile semantics.
// 1.29 publishes explicit closure capture modes, callable trait and native environment layout.
// 1.30 publishes verified must-tail call sites independently from optimization hints.
inline constexpr std::uint16_t kProjectOopMirSchemaMinor = 30;



const char* projectOopMirAbiPassingName(ProjectOopMirAbiPassing passing) noexcept;
















const char* projectOopMirCleanupKindName(ProjectOopMirCleanupKind kind) noexcept;





const char* projectOopMirNonlocalExitKindName(ProjectOopMirNonlocalExitKind kind) noexcept;
















class ProjectOopMirModel final
{
public:
    void setPanicProfile(language::PanicProfile profile) noexcept;
    language::PanicProfile panicProfile() const noexcept;
    void setPanicHandler(std::string handler);
    std::string_view panicHandler() const noexcept;
    std::string_view panicHandlerMangledName() const noexcept;
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectVirtualDispatchModel& virtualDispatchModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        std::string& error,
        std::size_t workerCount = 1);

    std::span<const ProjectOopMirFunction> functions() const noexcept;
    std::span<const ProjectOopMirFieldInitializer> fieldInitializers() const noexcept;
    std::span<const ProjectOopMirStaticField> staticFields() const noexcept;
    std::span<const ProjectOopMirStructLayout> structLayouts() const noexcept;
    std::span<const ProjectOopMirClassLayout> classLayouts() const noexcept;
    std::span<const ProjectOopMirDispatchTable> dispatchTables() const noexcept;
    std::span<const ProjectOopMirGenericSpecialization> genericSpecializations() const noexcept;
    std::span<const ProjectOopMirInterfaceBinding> interfaceBindings() const noexcept;
    const ProjectOopMirDispatchTable* dispatchTableForType(std::size_t typeOrdinal) const noexcept;
    const ProjectOopMirInterfaceBinding* interfaceBindingForTypes(
        std::size_t implementingTypeOrdinal,
        std::size_t interfaceTypeOrdinal) const noexcept;
    bool interfaceSlotForMember(
        std::size_t interfaceMemberOrdinal,
        std::uint64_t& stableInterfaceTypeId,
        std::size_t& slot) const noexcept;
    const ProjectOopMirFunction* functionForMember(std::size_t memberOrdinal) const noexcept;
    const ProjectOopMirFunction* functionForNamespaceFunction(
        std::size_t functionOrdinal) const noexcept;
    const ProjectOopMirFunction* executableEntryPoint(
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        bool& ambiguous) const noexcept;
    bool isBuilt() const noexcept;
    // NSC305-C04. Sets the MIR cache root (empty = disabled).
    void setMirCacheRoot(std::filesystem::path root);
    // NSC305-C04. Persists the built MIR functions keyed by implementation fingerprint (no-op when
    // disabled).
    void storeMirFunctions();
    // NSC305-C04. Validates/restores the MIR functions from the cache and counts hits (no-op when
    // disabled).
    void restoreMirFunctions();

private:
    friend struct ProjectOopHirModelTestAccess;

    class MirNodeMachine;
    class MirVerifier;

    bool importLayouts(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    void importNamedStructLayouts(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel);
    static void collectTupleTypeKey(
        std::string_view key,
        std::vector<std::string>& tupleKeys);
    static void collectTupleNodeKeys(
        const ProjectOopHirNode* root,
        std::vector<std::string>& tupleKeys);
    bool appendTupleLayout(
        const std::string& key,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::string& error);
    bool importTupleLayouts(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    void importClassLayouts(const ProjectObjectLayoutModel& objectLayoutModel);
    static std::uint64_t alignLayoutOffset(std::uint64_t value, std::uint64_t alignment);
    bool appendFutureLayout(
        const std::string& key,
        const ProjectNameLookup& nameLookup,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::string& error);
    bool importFutureLayouts(
        const ProjectNameLookup& nameLookup,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    bool appendIteratorLayout(
        const std::string& key,
        const ProjectNameLookup& nameLookup,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::string& error);
    bool importIteratorLayouts(
        const ProjectNameLookup& nameLookup,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    bool initializeBuild(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectVirtualDispatchModel& virtualDispatchModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    bool appendDispatchSlots(
        const ClassVirtualTable& sourceTable,
        const ProjectOopHirModel& hirModel,
        ProjectOopMirDispatchTable& table,
        std::string& error);
    bool appendDispatchDestructors(
        const ProjectMemberModel& memberModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        ProjectOopMirDispatchTable& table,
        std::string& error);
    bool appendDispatchInterfaces(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectOopHirModel& hirModel,
        std::string_view suffix,
        ProjectOopMirDispatchTable& table,
        std::string& error);
    bool appendDispatchTable(
        const ClassVirtualTable& sourceTable,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    bool buildDispatchTables(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectVirtualDispatchModel& virtualDispatchModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    bool appendOwnedFieldDestructionSteps(
        ProjectOopMirDispatchTable& table,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        const std::unordered_set<std::string>& destroyableClassTypeKeys,
        std::string& error);
    bool buildOwnedFieldDestructionPlans(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    bool lowerFieldInitializers(const ProjectOopHirModel& hirModel, std::string& error);
    bool appendStaticField(
        std::size_t memberOrdinal,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    bool materializeStaticFieldConstant(
        std::size_t memberOrdinal,
        const ClassMemberSymbol& field,
        const ProjectOopHirModel& hirModel,
        ProjectOopMirStaticField& staticField,
        std::string& error);
    bool buildStaticFields(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);
    void buildNamespaceStateFields(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel);
    static void collectFutureLayoutKeys(
        const ProjectOopHirNode* root,
        std::vector<std::string>& futureKeys);
    static void collectIteratorLayoutKeys(
        const ProjectOopHirNode* root,
        std::vector<std::string>& iteratorKeys);
    bool lowerFunctions(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectVirtualDispatchModel& virtualDispatchModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        std::string& error,
        std::size_t workerCount);
    void calculateMirCacheKeys(const ProjectOopHirModel& hirModel);
    void lookupMirCachePayloads();
    ProjectOopMirFunction lowerOneFunction(
        const ProjectOopHirFunction& source,
        const ProjectNameLookup& nameLookup,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::span<const ProjectOopMirStructLayout> functionLayouts,
        language::PanicProfile panicProfile,
        std::string& error);
    static bool collectCleanupContract(
        ProjectOopMirFunction& function,
        const ProjectOopMirNode* root,
        language::PanicProfile panicProfile,
        std::string& error);
    static bool validateCleanupReferences(
        const ProjectOopMirFunction& function,
        const ProjectOopMirNode* root,
        std::string& error);
    static void sumAwaitedFrameSize(
        const ProjectOopMirNode* root,
        std::span<const ProjectOopMirStructLayout> layouts,
        std::uint64_t& frameSize);
    static void collectPollLiveness(
        const ProjectOopMirNode* root,
        std::uint64_t& order,
        std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::unordered_map<std::size_t, std::uint64_t>& parameterLastUse,
        std::vector<std::uint64_t>& suspendOrders);
    static void collectMoveNextLiveness(
        const ProjectOopMirNode* root,
        std::uint64_t& order,
        std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::unordered_map<std::size_t, std::uint64_t>& parameterLastUse,
        std::vector<std::uint64_t>& yieldOrders);
    static void collectBorrowSources(
        const ProjectOopMirNode* root,
        std::unordered_map<std::size_t, std::size_t>& borrowSource,
        std::unordered_set<std::size_t>& borrowNonLocalSource);
    static void collectLivePollLocals(
        const ProjectOopMirNode* root,
        const std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        const std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::span<const std::uint64_t> suspendOrders,
        std::unordered_set<std::size_t>& liveSet);
    static void appendPollFrameLocals(
        const ProjectOopMirNode* root,
        const std::unordered_set<std::size_t>& liveSet,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectNameLookup& nameLookup,
        ProjectOopMirFunction& function,
        std::uint64_t& frameSize);
    static void appendMoveNextFrameLocals(
        const ProjectOopMirNode* root,
        const std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        const std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::span<const std::uint64_t> yieldOrders,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectNameLookup& nameLookup,
        ProjectOopMirFunction& function,
        std::uint64_t& frameSize);
    static std::string serializeMirNodeTreeForTesting(const ProjectOopMirNode* root);
    static bool restoreMirNodeTreeForTesting(
        std::string_view bytes,
        std::unique_ptr<ProjectOopMirNode>& root);
    void configureFunctionLowerer(
        ProjectOopMirModel& lowerer,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectVirtualDispatchModel& virtualDispatchModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel) const;

    // C2.1 "fixed arrays": size/alignment of one array element, for both the managed
    // (length-prefixed, heap) and the raw (inline storage / raw pointer) element paths -- one
    // answer, so a GEP stride can never disagree with the layout a field was given.
    std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout(
        std::string_view typeKey) const;

    std::unique_ptr<ProjectOopMirNode> lowerNode(
        const ProjectOopHirNode* node,
        std::string& error);
    const ObjectFieldLayout* fieldLayout(std::size_t memberOrdinal) const noexcept;
    const ObjectAllocationPlan* allocationPlan(std::size_t constructionOrdinal) const noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectObjectLayoutModel* objectLayoutModel_ = nullptr;
    const ProjectVirtualDispatchModel* virtualDispatchModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectDestructionModel* destructionModel_ = nullptr;
    const ProjectOopHirModel* hirModel_ = nullptr;
    std::vector<ProjectOopMirFunction> functions_;
    std::vector<ProjectOopMirFieldInitializer> fieldInitializers_;
    std::vector<ProjectOopMirStaticField> staticFields_;
    std::vector<ProjectOopMirStructLayout> structLayouts_;
    std::vector<ProjectOopMirClassLayout> classLayouts_;
    std::vector<std::optional<std::size_t>> functionByMember_;
    std::vector<std::optional<std::size_t>> functionByNamespaceFunction_;
    std::vector<ProjectOopMirDispatchTable> dispatchTables_;
    std::vector<std::optional<std::size_t>> dispatchTableByType_;
    std::vector<ProjectOopMirGenericSpecialization> genericSpecializations_;
    std::vector<ProjectOopMirInterfaceBinding> interfaceBindings_;
    bool built_ = false;
    language::PanicProfile panicProfile_ = language::PanicProfile::Abort;
    std::string panicHandler_;
    std::string panicHandlerMangledName_;
    // NSC305-C04. The MIR cache (disk I/O). Root empty = disabled.
    TemplateParseCache mirCache_;
    std::vector<std::string> mirCacheKeys_;
    std::vector<std::optional<std::string>> mirCachePayloads_;
};

const char* projectOopMirNodeKindName(ProjectOopMirNodeKind kind) noexcept;
}
