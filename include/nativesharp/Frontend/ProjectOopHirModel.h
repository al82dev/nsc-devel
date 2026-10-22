#pragma once

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nativesharp/Frontend/ProjectOopHirNodeKind.h"
#include "nativesharp/Frontend/ProjectCollectionSegmentKind.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"
#include "nativesharp/Frontend/ProjectOopHirFieldInitializer.h"
#include "nativesharp/Frontend/ProjectOopHirFunction.h"

namespace nativesharp::language
{
struct InterpolationPart;
}

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

namespace nativesharp::frontend
{
class ProjectGenericConstraintModel;
class ProjectMetaprogrammingModel;
class ProjectCompilationIndexes;
struct HirTuplePatternFrame;
struct ProjectOopHirModelTestAccess;
namespace internal
{
struct HirAggregateFrame;
struct BoundCallFrameState;
struct AllocatorConstructionFrameState;
struct AggregateCrossFrameState;
struct AggregateCrossStructuralFrame;
struct CoreOperandCallFrameState;
struct DeconstructionDeclarationFrameState;
struct EventInvokeFrameState;
struct ExitValueStatementFrameState;
struct InlineAssemblyFrameState;
struct MonitorCoreCallFrameState;
struct NamespaceCallFrameState;
struct OrdinaryCallFrameState;
struct OrdinaryConstructionFrameState;
struct RegularFunctionResult;
struct ReceiverFrameState;
struct IterativeExpressionFrame;
struct VariableDeclarationFrameState;
enum class IterativeExpressionKind : std::uint8_t;
enum class IterativeInputKind : std::uint8_t;
enum class LoopBlockAdvance : std::uint8_t;
}











class ProjectOopHirModel final
{
public:
    ProjectOopHirModel() = default;
    explicit ProjectOopHirModel(ProjectCompilationIndexes& indexes) noexcept;
    ~ProjectOopHirModel();

    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectMethodBodyModel& methodBodyModel,
        const ProjectConstructionModel& constructionModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::string& error,
        const ProjectGenericConstraintModel* genericConstraintModel = nullptr,
        std::size_t workerCount = 1,
        const ProjectNamespaceFunctionModel* namespaceFunctionModel = nullptr,
        const ProjectMetaprogrammingModel* metaprogrammingModel = nullptr);
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectMethodBodyModel& methodBodyModel,
        const ProjectConstructionModel& constructionModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::string& error,
        const ProjectGenericConstraintModel* genericConstraintModel,
        std::size_t workerCount,
        const ProjectNamespaceFunctionModel* namespaceFunctionModel,
        const ProjectMetaprogrammingModel* metaprogrammingModel,
        driver::CompilationWorkerPool* workerPool);

    std::span<const ProjectOopHirFunction> functions() const noexcept;
    std::span<const ProjectOopHirFieldInitializer> fieldInitializers() const noexcept;
    const ProjectOopHirFunction* functionForMember(std::size_t memberOrdinal) const noexcept;
    const ProjectOopHirFunction* functionForNamespaceFunction(
        std::size_t functionOrdinal) const noexcept;
    bool isBuilt() const noexcept;

    static std::string mangleCallable(
        const NamedTypeDeclaration& type,
        const ClassMemberSymbol& member);

private:
    friend struct ProjectOopHirModelTestAccess;
    friend struct internal::BoundCallFrameState;
    friend struct internal::IterativeExpressionFrame;

    bool buildImpl(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectMethodBodyModel& methodBodyModel,
        const ProjectConstructionModel& constructionModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::string& error,
        const ProjectGenericConstraintModel* genericConstraintModel,
        std::size_t workerCount,
        const ProjectNamespaceFunctionModel* namespaceFunctionModel,
        const ProjectMetaprogrammingModel* metaprogrammingModel,
        std::optional<std::size_t> memberFilter,
        const ProjectOopHirModel* lookupSource);
    bool initializeBuildState(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectMethodBodyModel& methodBodyModel,
        const ProjectConstructionModel& constructionModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectNamespaceFunctionModel* namespaceFunctionModel,
        std::string& error);
    void indexBuildBindings(
        const ProjectMethodBodyModel& methodBodyModel,
        const ProjectConstructionModel& constructionModel);
    bool buildRegularFunctionsInParallel(
        std::size_t workerCount,
        std::string& error);
    bool mergeParallelRegularFunctions(
        std::vector<internal::RegularFunctionResult>& results,
        std::string& error);
    bool buildRegularFunctionsSequentially(
        std::optional<std::size_t> memberFilter,
        std::string& error);
    bool buildRegularFunction(
        std::size_t memberOrdinal,
        std::string& error);
    bool regularFunctionShouldBeSkipped(
        std::size_t memberOrdinal,
        const ClassMemberSymbol& member) const;
    bool appendPrecompiledRegularFunction(
        std::size_t memberOrdinal,
        const ClassMemberSymbol& member,
        std::string& error);
    void appendImportedCRegularFunction(
        std::size_t memberOrdinal,
        const ClassMemberSymbol& member);
    void initializeRegularFunction(
        ProjectOopHirFunction& function,
        std::size_t memberOrdinal,
        const ClassMemberSymbol& member);
    bool appendRegularFunctionParameterCleanups(
        ProjectOopHirFunction& function,
        const ClassMemberSymbol& member,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& cleanupNodes,
        std::string& error);
    bool prependConstructorWork(
        ProjectOopHirFunction& function,
        std::size_t memberOrdinal,
        const ClassMemberSymbol& member,
        std::string& error);
    bool appendConstructorInitializerCall(
        std::vector<std::unique_ptr<ProjectOopHirNode>>& prefix,
        std::size_t memberOrdinal,
        const ConstructorInitializerBinding* binding,
        std::string& error);
    bool appendConstructorFieldInitializerWork(
        std::vector<std::unique_ptr<ProjectOopHirNode>>& prefix,
        std::size_t memberOrdinal,
        const ClassMemberSymbol& member,
        const ConstructorInitializerBinding* binding,
        std::string& error);
    bool appendConstructorInitializerArguments(
        ProjectOopHirNode& initializerCall,
        const ConstructorInitializerBinding& binding,
        const ClassMemberSymbol& selectedConstructor,
        std::size_t memberOrdinal,
        bool& hasOwnedTemporary,
        std::string& error);
    bool appendConstructorInitializerDefaults(
        ProjectOopHirNode& initializerCall,
        const ConstructorInitializerBinding& binding,
        const ClassMemberSymbol& selectedConstructor,
        std::string& error);
    void promoteEligibleStackClassLocals(
        ProjectOopHirFunction& function,
        const std::vector<ProjectOopHirNode*>& localDeclarations);
    void promoteScopeClassLocals(
        const std::vector<ProjectOopHirNode*>& localDeclarations);
    bool validateNoHeapRegularFunction(
        const ProjectOopHirFunction& function,
        const ClassMemberSymbol& member,
        std::string& error) const;
    bool appendRegularCoroutineFunctions(
        ProjectOopHirFunction function,
        std::string& error);
    bool buildNamespaceFunctions(std::string& error);
    bool buildNamespaceFunction(
        std::size_t functionOrdinal,
        std::string& error);
    void initializeNamespaceFunction(
        ProjectOopHirFunction& function,
        std::size_t functionOrdinal,
        const NamespaceFunctionSymbol& symbol) const;
    bool appendNamespaceFunctionParameterCleanups(
        ProjectOopHirFunction& function,
        std::size_t callableOrdinal,
        const NamespaceFunctionSymbol& symbol,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& cleanupNodes,
        std::string& error);
    bool buildLambdaFunctions(std::string& error);
    bool buildLambdaFunction(
        const MethodLambdaBinding& lambda,
        std::string& error);
    bool lowerExpressionLambdaBody(
        ProjectOopHirFunction& function,
        const MethodLambdaBinding& lambda,
        std::string& error);
    bool markLambdaClosureCells(std::string& error);
    bool buildFieldInitializers(std::string& error);
    bool buildFieldInitializer(
        std::size_t memberOrdinal,
        std::string& error);
    bool buildImplicitConstructors(std::string& error);
    bool buildImplicitConstructor(
        std::size_t typeOrdinal,
        std::string& error);

    static bool stackClassLocalUsesAreSafe(
        const ProjectOopHirNode* node,
        const ProjectOopHirNode* declaration,
        std::size_t localOrdinal,
        const ProjectOopHirNode* parent = nullptr,
        const ProjectOopHirNode* grandparent = nullptr,
        std::size_t childIndex = 0,
        std::size_t parentChildIndex = 0);
    static void collectLocalDeclarations(
        ProjectOopHirNode* node,
        std::vector<ProjectOopHirNode*>& declarations);
    static const ProjectOopHirNode* firstUnpromotedObjectConstruction(
        const ProjectOopHirNode* node);
    static void scanParallelOrdinals(
        const ProjectOopHirNode* node,
        std::size_t localFloor,
        std::size_t cleanupFloor,
        std::size_t& localCount,
        std::size_t& cleanupCount,
        std::size_t& regionCount);
    static void applyParallelOrdinalOffsets(
        ProjectOopHirNode* node,
        std::size_t localFloor,
        std::size_t cleanupFloor,
        std::size_t localOffset,
        std::size_t cleanupOffset,
        std::size_t regionOffset);

    std::unique_ptr<ProjectOopHirNode> lowerBlock(
        const ast::AstNode* block,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    // EL ENVOLTORIO QUE RECUERDA DONDE FALLO, y por que es un envoltorio y no 125 ediciones.
    //
    // Este fichero tiene 125 sitios que hacen `error = "..."; return nullptr;`, y ninguno decia en
    // que parte del programa del usuario estaba el problema. Adjuntar la ubicacion en cada uno
    // seria 125 ediciones, cada una necesitando el nodo correcto a mano.
    //
    // En vez de eso: `lowerExpression` es el embudo por el que pasan casi todos. El envoltorio
    // llama al trabajo real y, si vuelve en fallo y todavia no hay rango apuntado, apunta el del
    // nodo QUE ESTABA BAJANDO. Como el fallo se propaga desde dentro hacia fuera y solo se apunta
    // si no hay nada apuntado, el que gana es el nodo MAS INTERNO -- el que de verdad no se pudo
    // bajar, no la expresion entera que lo contiene.
    //
    // Es aproximado a proposito: apunta a un nodo, no al subtermino exacto en los casos donde el
    // error se detecta un nivel mas arriba. Eso es infinitamente mas que nada, cuesta una edicion
    // y no toca el camino de exito.
    std::unique_ptr<ProjectOopHirNode> lowerExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);

    // Lowers an expression statement through an explicit completion frame so
    // returning from the child expression does not require a native call frame.
    std::unique_ptr<ProjectOopHirNode> lowerExpressionStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerVariableDeclarationStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDeconstructionDeclarationStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerInlineAssemblyStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerStaticAssertStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishStaticAssertStatement(
        const ast::AstNode* statement,
        std::unique_ptr<ProjectOopHirNode> condition,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerIfStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerLoopStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerSwitchStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerForStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerForeachStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerExitValueStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerUsingStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerTryStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);

    // Runs the expression worklist with an optional statement wrapper at its
    // bottom; a null wrapper preserves the ordinary expression entry point.
    std::unique_ptr<ProjectOopHirNode> lowerExpressionWithStatementFrame(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const ast::AstNode* statementContinuation,
        std::string& error);
    bool runRootExpressionWorklist(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        bool& worklistComplete,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::IterativeExpressionKind> iterativeExpressionKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeDelegateAndCallKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeCoreCallKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeSpecialCallKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeConstructionKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeOperatorKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeControlFlowKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeSetterAssignmentKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeExitAndDestructionKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeStorageKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeMemberKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeIndexKind(
        const ast::AstNode* syntax,
        bool address);
    std::optional<internal::IterativeExpressionKind> iterativeUserIndexAndFieldKind(
        const ast::AstNode* syntax,
        bool address);
    static bool validIterativeIndexSyntax(const ast::AstNode* index) noexcept;
    static bool validRawIterativeIndexSyntax(const ast::AstNode* index) noexcept;
    static bool isInterpolationHole(
        const language::InterpolationPart& part) noexcept;
    static bool isSpreadElement(const ast::AstNode* child) noexcept;
    bool applyExpressionBoundaryPostconditions(
        std::unique_ptr<ProjectOopHirNode>& child,
        const ast::AstNode* childSyntax,
        std::size_t childMemberOrdinal,
        std::string& error);
    void applyRootExpressionPostconditions(
        std::unique_ptr<ProjectOopHirNode>& lowered,
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        const ast::AstNode* statementContinuation,
        std::string& error);
    static const ProjectOopHirNode* rootLoweredExpression(
        const std::unique_ptr<ProjectOopHirNode>& lowered,
        const ast::AstNode* statementContinuation) noexcept;
    static bool statementChildPolicyApplied(
        const ast::AstNode* statementContinuation) noexcept;
    static bool directlyAllocatesHeap(ProjectOopHirNodeKind kind) noexcept;
    void rejectDirectNoHeapAllocation(
        std::unique_ptr<ProjectOopHirNode>& child,
        const ClassMemberSymbol& activeMember,
        std::string& error);
    void rejectNoHeapBoundCall(
        std::unique_ptr<ProjectOopHirNode>& child,
        const ast::AstNode* childSyntax,
        const ClassMemberSymbol& activeMember,
        std::string& error);
    static void rejectNoHeapIndirectInvocation(
        std::unique_ptr<ProjectOopHirNode>& child,
        const ClassMemberSymbol& activeMember,
        std::string& error);
    void rejectNoHeapConstruction(
        std::unique_ptr<ProjectOopHirNode>& child,
        const ast::AstNode* childSyntax,
        const ClassMemberSymbol& activeMember,
        std::string& error);
    bool initializeSwitchStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::string& error);
    void initializeForStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement);
    bool initializeForeachStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::string& error);
    bool initializeExitValueStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::string& error);
    bool initializeBreakValueStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::string& error);
    bool initializeReturnValueStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::string& error);
    bool initializeThrowValueStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::string& error);
    void initializeCoroutineValueStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::size_t statementMemberOrdinal);
    bool initializeUsingStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::string& error);
    void initializeTryStatementFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement);
    static bool isBoundCallKind(
        internal::IterativeExpressionKind kind) noexcept;
    std::optional<bool> initializeReceiverFrame(
        internal::ReceiverFrameState& receiver,
        const ast::AstNode* receiverSyntax,
        std::string_view receiverTypeKey,
        std::string& error);
    void acceptReceiverChild(
        internal::ReceiverFrameState& receiver,
        std::unique_ptr<ProjectOopHirNode> child);
    bool initializeBoundCallFrame(
        internal::IterativeExpressionFrame& frame,
        const MethodCallBinding* binding,
        std::vector<const ast::AstNode*> arguments,
        bool nullConditional,
        std::string& error);
    bool nextBoundCallChild(
        const internal::BoundCallFrameState& call,
        const ast::AstNode*& syntax,
        bool& addressContext);
    void acceptBoundCallChild(
        internal::BoundCallFrameState& call,
        std::unique_ptr<ProjectOopHirNode> child);
    bool validateOrdinaryCallMember(
        const internal::OrdinaryCallFrameState& call,
        std::string& error) const;
    bool initializeOrdinaryCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    void redirectOrdinaryInterfaceCall(
        internal::OrdinaryCallFrameState& call,
        bool& interfaceDispatch) const;
    void initializeOrdinaryCallNode(
        internal::OrdinaryCallFrameState& call,
        internal::IterativeExpressionFrame& frame,
        bool interfaceDispatch);
    bool scheduleOrdinaryCallFrame(
        internal::OrdinaryCallFrameState& call,
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool nextOrdinaryCallChild(
        const internal::OrdinaryCallFrameState& call,
        const ast::AstNode*& syntax,
        bool& addressContext) const;
    bool acceptOrdinaryCallChild(
        internal::OrdinaryCallFrameState& call,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool acceptOrdinaryCallReceiverChild(
        internal::OrdinaryCallFrameState& call,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool acceptOrdinaryCallArgumentChild(
        internal::OrdinaryCallFrameState& call,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishOrdinaryCall(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeNamespaceCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    static bool nextNamespaceCallChild(
        const internal::NamespaceCallFrameState& call,
        const ast::AstNode*& syntax,
        bool& addressContext);
    bool acceptNamespaceCallChild(
        internal::NamespaceCallFrameState& call,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool acceptNamespaceCallReceiverChild(
        internal::NamespaceCallFrameState& call,
        std::unique_ptr<ProjectOopHirNode> child);
    bool acceptNamespaceCallArgumentChild(
        internal::NamespaceCallFrameState& call,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool initializeEventInvokeFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool nextEventInvokeChild(
        const internal::EventInvokeFrameState& call,
        const ast::AstNode*& syntax,
        bool& addressContext);
    bool acceptEventInvokeChild(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool initializeDelegateInvokeFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    static bool nextDelegateInvokeChild(
        const internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& syntax);
    void acceptDelegateInvokeChild(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child);
    bool initializeAssertionCoreFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeIteratorCoreCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeCoreOperandCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeSpanCoreOperandCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::CoreOperandCallFrameState& call,
        std::string& error);
    bool initializeStringCoreOperandCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::CoreOperandCallFrameState& call,
        std::string& error);
    static bool nextCoreOperandCallChild(
        const internal::CoreOperandCallFrameState& call,
        const ast::AstNode*& syntax,
        bool& addressContext);
    void acceptCoreOperandCallChild(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child);
    bool initializeAtomicCoreCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    static bool nextAtomicCoreCallChild(
        const internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& syntax);
    bool acceptAtomicCoreCallChild(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool initializeMonitorCoreCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    static bool nextMonitorCoreCallChild(
        const internal::MonitorCoreCallFrameState& call,
        const ast::AstNode*& syntax);
    void acceptMonitorCoreCallChild(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child);
    bool initializeConsoleOutputCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeConsoleReadLineCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeEnumToStringCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeStructCloneCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeStructAggregateCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeEnvironmentExitCallFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool prepareOrdinaryConstructionTail(
        internal::OrdinaryConstructionFrameState& construction,
        std::string& error);
    bool appendOrdinaryConstructionDefaults(
        internal::OrdinaryConstructionFrameState& construction,
        std::string& error);
    bool appendEmptyOrdinaryConstructionPack(
        internal::OrdinaryConstructionFrameState& construction,
        std::string& error);
    void collectImplicitOrdinaryConstructionFields(
        internal::OrdinaryConstructionFrameState& construction);
    bool initializeOrdinaryConstructionFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool nextOrdinaryConstructionChild(
        const internal::OrdinaryConstructionFrameState& construction,
        const ast::AstNode*& syntax) const;
    bool acceptOrdinaryConstructionChild(
        internal::OrdinaryConstructionFrameState& construction,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool acceptOrdinaryConstructionArgument(
        internal::OrdinaryConstructionFrameState& construction,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool appendOrdinaryConstructionPackElement(
        internal::OrdinaryConstructionFrameState& construction,
        std::unique_ptr<ProjectOopHirNode> child,
        const ast::AstNode* argumentSyntax,
        std::size_t parameterIndex,
        std::string& error);
    bool acceptOrdinaryConstructionFieldInitializer(
        internal::OrdinaryConstructionFrameState& construction,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishOrdinaryConstructionFrame(
        internal::IterativeExpressionFrame& frame);
    bool initializeAllocatorConstructionFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    static bool nextAllocatorConstructionChild(
        const internal::AllocatorConstructionFrameState& construction,
        const ast::AstNode*& syntax,
        bool& addressContext);
    bool acceptAllocatorConstructionChild(
        internal::AllocatorConstructionFrameState& construction,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishAllocatorConstructionFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool prepareAggregateCrossStructuralFrame(
        internal::AggregateCrossStructuralFrame& structural,
        std::string& error);
    bool prepareTupleAggregateCrossStructuralFrame(
        internal::AggregateCrossStructuralFrame& structural,
        const std::vector<std::string>& tupleElements,
        std::string& error);
    bool prepareFixedArrayAggregateCrossStructuralFrame(
        internal::AggregateCrossStructuralFrame& structural,
        std::string_view elementTypeKey,
        std::string& error);
    bool prepareStructAggregateCrossStructuralFrame(
        internal::AggregateCrossStructuralFrame& structural,
        std::string& error);
    bool advanceAggregateCrossFrame(
        internal::AggregateCrossFrameState& aggregate,
        std::string& error);
    bool advanceAggregateCrossEntry(
        internal::AggregateCrossFrameState& aggregate,
        internal::AggregateCrossStructuralFrame& structural,
        bool& continueStructural,
        std::string& error);
    bool initializeAggregateCrossFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    static bool nextAggregateCrossChild(
        const internal::AggregateCrossFrameState& aggregate,
        const ast::AstNode*& syntax,
        bool& addressContext);
    bool acceptAggregateCrossChild(
        internal::AggregateCrossFrameState& aggregate,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool acceptAggregateWithSource(
        internal::AggregateCrossFrameState& aggregate,
        const internal::AggregateCrossStructuralFrame& structural,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool acceptAggregateSpreadSource(
        internal::AggregateCrossFrameState& aggregate,
        const internal::AggregateCrossStructuralFrame& structural,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    void acceptAggregateValue(
        internal::AggregateCrossFrameState& aggregate,
        std::unique_ptr<ProjectOopHirNode> child);
    static std::unique_ptr<ProjectOopHirNode> finishAggregateCrossFrame(
        internal::IterativeExpressionFrame& frame);
    std::optional<bool> prepareSetterReceiver(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeVariableDeclarationFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool initializeVariableCleanup(
        internal::VariableDeclarationFrameState& variable,
        const ast::AstNode* syntax,
        std::string& error);
    bool initializeVariableMode(
        internal::VariableDeclarationFrameState& variable,
        const ast::AstNode* syntax,
        std::string& error);
    static bool nextVariableDeclarationChild(
        const internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& syntax,
        bool& addressContext);
    void completeVariableAllocation(
        internal::IterativeExpressionFrame& frame);
    bool acceptVariableDeclarationChild(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    bool acceptVariableInitializer(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child);
    void acceptVariableExtent(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child);
    static std::unique_ptr<ProjectOopHirNode> finishVariableDeclarationFrame(
        internal::IterativeExpressionFrame& frame);
    bool initializeDeconstructionDeclarationFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool nextDeconstructionDeclarationChild(
        const internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& syntax,
        bool& addressContext);
    bool acceptDeconstructionDeclarationChild(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishDeconstructionDeclarationFrame(
        internal::IterativeExpressionFrame& frame,
        std::string& error);
    bool expandDeconstructionBindingTuple(
        std::string& error,
        const ast::AstNode* bindingSyntax,
        const std::string& typeKey,
        std::uint64_t byteOffset,
        std::vector<HirTuplePatternFrame>& children);
    bool appendDeconstructionBindingLeaf(
        ProjectOopHirNode& block,
        ProjectOopHirNode& deconstruct,
        std::string& error,
        const ast::AstNode* bindingSyntax,
        const std::string& typeKey,
        std::uint64_t byteOffset);
    void initializeInlineAssemblyFrame(
        internal::IterativeExpressionFrame& frame);
    bool advanceInlineAssemblyFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& syntax,
        bool& addressContext,
        std::string& error);
    bool acceptInlineAssemblyOperand(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> child,
        std::string& error);
    static std::unique_ptr<ProjectOopHirNode> finishInlineAssemblyFrame(
        internal::IterativeExpressionFrame& frame);
    void rollbackMatchCleanups(
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    bool resumePendingConditionStatementAfterFailure(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error);
    void activateLocalCleanup(
        const std::unique_ptr<ProjectOopHirNode>& node);
    void activateDirectStatementCleanup(
        const std::unique_ptr<ProjectOopHirNode>& node);
    static bool isExitValueStatement(
        const ast::AstNode* statement) noexcept;
    static bool isCoroutineValueStatement(
        const ast::AstNode* statement) noexcept;
    bool beginExitValueStatement(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        const ast::AstNode* statement,
        std::size_t statementMemberOrdinal);
    void beginCoroutineValueStatement(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        const ast::AstNode* statement,
        std::size_t statementMemberOrdinal);
    bool beginUsingStatement(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        const ast::AstNode* statement,
        std::size_t statementMemberOrdinal);
    void beginTryStatement(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        const ast::AstNode* statement,
        std::size_t statementMemberOrdinal);
    bool initializeStatementContinuationFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statementContinuation,
        std::size_t memberOrdinal,
        bool& addressContext,
        std::string& error);
    std::optional<bool> initializeDeclarationContinuationFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statementContinuation);
    std::optional<bool> initializeControlContinuationFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statementContinuation,
        std::string& error);
    std::optional<bool> initializeValueContinuationFrame(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statementContinuation,
        std::size_t memberOrdinal,
        bool& addressContext,
        std::string& error);
    bool initializeRootLoopWorklist(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        bool& worklistComplete,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    bool initializeRootDeclarationWorklist(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        bool& worklistComplete,
        const ast::AstNode* statementContinuation);
    bool initializeRootVariableWorklist(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        bool& worklistComplete);
    bool initializeRootDeconstructionWorklist(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        bool& worklistComplete,
        const ast::AstNode* statementContinuation);
    bool initializeRootInlineAssemblyWorklist(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        bool& worklistComplete);
    bool expandDeconstructionAssignmentTargetTuple(
        std::string& error,
        const ast::AstNode* targetSyntax,
        const std::string& sourceTypeKey,
        std::uint64_t byteOffset,
        std::vector<HirTuplePatternFrame>& children);
    static bool collectDeconstructionAssignmentTargetLeaf(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* targetSyntax,
        const std::string& sourceTypeKey,
        std::uint64_t byteOffset);
    void materializeSetterReceiver(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    bool beginMatchArm(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::string& error);
    bool advanceMatchDeclarations(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceMatchVariableDeclaration(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* declaration,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::string& error);
    std::optional<internal::LoopBlockAdvance>
    advanceMatchDeconstructionDeclaration(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* declaration,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceMatchInlineAssembly(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* declaration,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceMatchConditionalOrLoop(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* declaration,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext);
    std::optional<internal::LoopBlockAdvance> advanceMatchSwitchOrIteration(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* declaration,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceMatchValueStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* declaration,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    bool beginMatchValue(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    void finishMatch(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    internal::LoopBlockAdvance advanceForStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t frameIndex);
    internal::LoopBlockAdvance advanceForInitializer(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t frameIndex);
    internal::LoopBlockAdvance advanceForCondition(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    internal::LoopBlockAdvance advanceForIncrement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    internal::LoopBlockAdvance advanceForBody(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    internal::LoopBlockAdvance advanceSwitchStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t frameIndex);
    std::optional<internal::LoopBlockAdvance> beginNextSwitchSection(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error);
    const ast::AstNode* nextSwitchSectionStatement(
        internal::IterativeExpressionFrame& frame);
    std::optional<internal::LoopBlockAdvance> advanceSwitchSimpleStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    std::optional<internal::LoopBlockAdvance> advanceSwitchDeclarationStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceSwitchVariableStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceSwitchDeconstructionStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceSwitchAssemblyStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceSwitchControlStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceSwitchConditionalStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    std::optional<internal::LoopBlockAdvance> advanceSwitchLoopStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceSwitchIterationStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceSwitchValueStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    internal::LoopBlockAdvance advanceLoopBlockFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t frameIndex);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockSimpleStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockDeclarationStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockVariableStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockDeconstructionStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockAssemblyStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockControlStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockConditionalStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockLoopStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockIterationStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t frameIndex);
    std::optional<internal::LoopBlockAdvance> advanceLoopBlockValueStatement(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* statement,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    internal::LoopBlockAdvance advanceForeachStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t frameIndex);
    std::optional<internal::LoopBlockAdvance> advanceForeachSimpleSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceForeachBasicSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    std::optional<internal::LoopBlockAdvance> advanceForeachDeclarationSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceForeachAssemblySource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceForeachControlSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t& frameIndex);
    std::optional<internal::LoopBlockAdvance> advanceForeachConditionalSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceForeachSwitchSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> advanceForeachIterationSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t& frameIndex);
    std::optional<internal::LoopBlockAdvance> advanceForeachValueSource(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode* source,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> initializeScheduledPrimaryFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledLoopExpressionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledAwaitExpressionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledUserUnaryOperatorFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledUserBinaryOperatorFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledUserConversionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledNamespaceCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledEventInvokeFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledDelegateInvokeFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> initializeScheduledCoreFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledAssertionCoreFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledIteratorCoreCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledCoreOperandCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledAtomicCoreCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledMonitorCoreCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledConsoleOutputCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledConsoleReadLineCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledEnumToStringCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> initializeScheduledCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledStructCloneCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledStructAggregateCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledEnvironmentExitCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledOrdinaryCallFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledAggregateCrossLoweringFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledOrdinaryConstructionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledAllocatorConstructionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledPropertyGetterFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> initializeScheduledAccessFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledExtensionPropertyGetterFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledUserIndexerGetterFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledDelegateCreationReceiverFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledInterpolationHandlerFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledCollectionBuilderFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledArrayConstructionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledInstanceFieldFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledRangeSliceFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> initializeScheduledValueFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledCoreIndexFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledCoreBinaryFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledRawIndexFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledResultPropagationFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledDestructionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledStackAllocationFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledMoveProjectionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledMatchFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> initializeScheduledAssignmentFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledDeconstructionAssignmentFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledEventSubscriptionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledPropertyIndexerSetterFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledNamespaceExtensionSetterFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledNullConditionalFieldAssignmentFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledNullConditionalArrayAssignmentFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance initializeScheduledDirectAssignmentFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    internal::LoopBlockAdvance initializeScheduledExpressionFrame(
        internal::IterativeExpressionFrame& frame,
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledLoopAwaitChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledNamespaceEventChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledDelegateConsoleChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledCoreOperandChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledAtomicMonitorChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledOrdinaryCallChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledAggregateChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledOrdinaryConstructionChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledAllocatorChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledReceiverChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledRemainingChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    internal::LoopBlockAdvance selectScheduledChildSyntax(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledPrimaryChildSyntax(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> selectScheduledSecondaryChildSyntax(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    void configureScheduledChildContext(
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    bool configureScheduledPrimaryChildContext(
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind);
    bool configureScheduledValueChildContext(
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        bool& currentAddressContext);
    bool configureScheduledCallChildContext(
        internal::IterativeExpressionKind currentKind,
        std::vector<internal::IterativeExpressionFrame>& frames,
        bool& currentAddressContext);
    void configureScheduledFallbackChildContext(
        internal::IterativeExpressionKind currentKind,
        bool& currentAddressContext);

    internal::LoopBlockAdvance selectScheduledExpressionChild(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    internal::LoopBlockAdvance scheduleIterativeExpression(
        internal::IterativeExpressionKind currentKind,
        bool& completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    internal::LoopBlockAdvance scheduleCurrentExpression(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance lowerScheduledDirectExpression(
        bool completedWithoutChild,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode* current,
        std::size_t currentMemberOrdinal,
        bool currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    bool scheduledChildUsesExpressionImpl(
        const std::vector<internal::IterativeExpressionFrame>& frames) const;
    void setScheduledLoweringErrorRange(
        const std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode* current);

    bool runExpressionScheduler(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        bool& worklistComplete,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> completeScheduledTerminalFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledResultPropagationFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledDestructionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledStackAllocationFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledStorageUnaryFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    void finishStorageUnaryFrame(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    internal::LoopBlockAdvance completeScheduledMoveProjectionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    std::optional<internal::LoopBlockAdvance> completeScheduledStatementFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> completeScheduledStatementFramesGroup1(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> completeScheduledStatementFramesGroup2(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledVariableDeclarationFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledInlineAssemblyStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledStaticAssertStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledIfStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeIfStatementAlternative(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> beginIfControlAlternative(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode* alternative,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> beginIfIterationAlternative(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode* alternative,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> beginIfValueAlternative(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode* alternative,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledLoopStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledSwitchStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledForStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledExitValueStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    void completeBreakExitValue(
        internal::IterativeExpressionFrame& frame,
        internal::ExitValueStatementFrameState& state,
        const ast::AstNode* valueSyntax,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    internal::LoopBlockAdvance completeReturnExitValue(
        internal::IterativeExpressionFrame& frame,
        internal::ExitValueStatementFrameState& state,
        const ast::AstNode* valueSyntax,
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledCoroutineValueStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledUsingStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledTryStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> completeTryStatementRegion(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> beginNextTryCatch(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> beginTryFinally(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledForeachStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledLoopBlockExpressionStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledLoopBlockFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    std::optional<internal::LoopBlockAdvance> completeScheduledSimpleFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledLoopExpressionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledAwaitExpressionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledExpressionStatementFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledOverflowPolicyFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledBuiltinUnaryFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledDelegateCreationReceiverFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledConversionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledMatchFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeInitialMatchFrame(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeMatchGuardAndValueFrame(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeMatchDeclarationFrame(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance finishScheduledMatchArm(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    std::optional<internal::LoopBlockAdvance> completeScheduledAccessFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledCoreMemberFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledCoreIndexFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    bool scheduleNextCoreIndexOperand(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    void finishCoreIndexFrame(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    internal::LoopBlockAdvance completeScheduledRawIndexFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    bool scheduleNextRawIndexChild(
        internal::IterativeExpressionFrame& frame,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    void finishVariableRawIndex(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    std::unique_ptr<ProjectOopHirNode> loadRawIndexAddress(
        const internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode> address);
    void finishArrayRawIndex(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    void applyRawIndexTrailingBounds(
        ProjectOopHirNode& address,
        const internal::IterativeExpressionFrame& frame);
    internal::LoopBlockAdvance completeScheduledNamespaceCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledEventInvokeFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledDelegateInvokeFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    std::optional<internal::LoopBlockAdvance> completeScheduledCoreCallFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledAssertionCoreFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledIteratorCoreCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledCoreOperandCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledAtomicCoreCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledMonitorCoreCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledConsoleOutputCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledConsoleReadLineCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledEnumToStringCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledStructCloneCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledStructAggregateCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledEnvironmentExitCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    std::optional<internal::LoopBlockAdvance> completeScheduledConstructionFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledOrdinaryCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledAggregateCrossLoweringFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledOrdinaryConstructionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledAllocatorConstructionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledExtensionPropertyGetterFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledInterpolationHandlerFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledCollectionBuilderFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledArrayConstructionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    std::optional<internal::LoopBlockAdvance> completeScheduledValueFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledInstanceFieldFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledNullConditionalFieldFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledRangeSliceFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    void finishRangeSliceFrame(
        internal::IterativeExpressionFrame& frame,
        const MethodRangeSliceBinding& range,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    internal::LoopBlockAdvance completeScheduledBuiltinBinaryFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledCoreBinaryFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    void finishSimpleCoreBinaryFrame(
        internal::IterativeExpressionFrame& frame,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    internal::LoopBlockAdvance completeScheduledNullCoalescingFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledNullCoalescingAssignmentFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledDeconstructionAssignmentFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    bool initializeDeconstructionAssignmentCompletion(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error);

    std::optional<internal::LoopBlockAdvance> completeScheduledAssignmentFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledEventSubscriptionFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    bool beginEventSubscriptionValue(
        internal::IterativeExpressionFrame& frame,
        const MethodEventBinding& binding,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    void finishEventSubscription(
        internal::IterativeExpressionFrame& frame,
        const MethodEventBinding& binding,
        std::unique_ptr<ProjectOopHirNode>& lowered);
    internal::LoopBlockAdvance completeScheduledPropertyIndexerSetterFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeCompoundSetterValue(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error);
    bool finishPropertyIndexerSetterFrame(
        internal::IterativeExpressionFrame& frame,
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error);
    internal::LoopBlockAdvance completeScheduledNamespaceExtensionSetterFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledNullConditionalFieldAssignmentFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledNullConditionalArrayAssignmentFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledDirectAssignmentFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledStringCompoundAssignmentFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    internal::LoopBlockAdvance completeScheduledCompoundAssignmentFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);

    internal::LoopBlockAdvance completeExpressionFrames(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> completeExpressionFrameGroup1(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> completeExpressionFrameGroup2(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        internal::IterativeInputKind& currentInputKind,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance>
    completeScheduledDeconstructionDeclarationFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error,
        std::size_t rootCleanupDepth,
        std::size_t rootExceptionHandlerDepth);
    std::optional<internal::LoopBlockAdvance> completeScheduledBoundCallFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered,
        std::string& error);
    std::optional<internal::LoopBlockAdvance> completeScheduledConditionalFrame(
        std::vector<internal::IterativeExpressionFrame>& frames,
        const ast::AstNode*& current,
        std::size_t& currentMemberOrdinal,
        bool& currentAddressContext,
        std::unique_ptr<ProjectOopHirNode>& lowered);

    std::unique_ptr<ProjectOopHirNode> lowerExpressionImpl(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerExpressionImplGroup1(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerExpressionImplGroup2(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerExpressionImplGroup3(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerLambdaCreationExpressionImplCase(
        const ast::AstNode* expression,
        const MethodLambdaBinding* stage43Lambda,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDelegateCreationExpressionImplCase(
        const ast::AstNode* expression,
        const MethodDelegateBinding* stage42Delegate,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerLiteralExpressionImplCase(
        const ast::AstNode* expression,
        bool addressContext,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerOverflowPolicyExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerConditionalExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerResolvedCall(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error,
        const MethodExpressionTypeBinding* typeBinding,
        const MethodCallBinding* binding,
        const std::vector<const ast::AstNode*>& argumentSyntaxes,
        std::unique_ptr<ProjectOopHirNode> preloweredLast = nullptr,
        bool nullConditional = false);
    std::unique_ptr<ProjectOopHirNode> lowerEventBacking(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::string& error,
        const MethodEventBinding* binding,
        bool address);
    std::unique_ptr<ProjectOopHirNode> lowerThisExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNullConditionalMemberExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNameExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerCoreNameExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodCoreBinding& core,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerLambdaNameExpression(
        const ast::AstNode* expression,
        bool addressContext);
    std::unique_ptr<ProjectOopHirNode> lowerParameterNameBinding(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodNameBinding& binding);
    std::unique_ptr<ProjectOopHirNode> lowerLocalNameBinding(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodNameBinding& binding);
    std::unique_ptr<ProjectOopHirNode> lowerFieldNameBinding(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodNameBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNamespaceStateNameBinding(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodNameBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerStaticFieldNameBinding(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodNameBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerBoundNameExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodNameBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerInterpolatedStringExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerUnaryExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerIncrementDecrementExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNullCoalescingExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerBinaryExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerStructEqualityExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerIndexExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerRangeSliceIndexExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodRangeSliceBinding* range,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerRangeEndpoint(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        const ast::AstNode* syntax,
        const ProjectImplicitConversion& conversion,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerArrayOrStringIndexExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodCoreBinding* core,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNullConditionalArrayIndexExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodCoreBinding* core,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNullConditionalUserIndexExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerRawElementIndexExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodCoreBinding* core,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerVariableArrayIndexExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodCoreBinding* core,
        const MethodVariableArrayIndexBinding* variableArrayIndex,
        std::unique_ptr<ProjectOopHirNode> base,
        const std::vector<const ast::AstNode*>& indices,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerOrdinaryRawElementIndexExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodCoreBinding* core,
        const MethodExpressionTypeBinding* baseType,
        bool baseIsFixedArray,
        bool baseIsFlexibleArray,
        std::unique_ptr<ProjectOopHirNode> base,
        const std::vector<const ast::AstNode*>& indices,
        std::string& error);
    void configureFlexibleArrayBounds(
        ProjectOopHirNode* address,
        const ast::AstNode* baseSyntax,
        bool baseIsFlexibleArray);
    std::unique_ptr<ProjectOopHirNode> lowerIndexerAccessorExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        const MethodCallBinding* indexerCall,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerAssignmentExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNullCoalescingAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    bool expandAssignmentTargetTuple(
        std::string& error,
        const ast::AstNode* targetSyntax,
        const std::string& sourceTypeKey,
        std::uint64_t byteOffset,
        std::vector<HirTuplePatternFrame>& children);
    bool appendAssignmentTargetLeaf(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        ProjectOopHirNode* node,
        std::string& error,
        const ast::AstNode* targetSyntax,
        const std::string& sourceTypeKey,
        std::uint64_t byteOffset);
    std::unique_ptr<ProjectOopHirNode> lowerTupleDeconstructionAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerEventAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodEventBinding* stage42Event,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerCompoundSetterAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        const MethodCallBinding* setterCall,
        const std::vector<const ast::AstNode*>& arguments,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerSetterAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodExpressionTypeBinding* typeBinding,
        const MethodCallBinding* setterCall,
        const ClassMemberSymbol& setter,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNamespaceSetterAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const NamespaceFunctionCallBinding* namespaceSetter,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNullConditionalMemberAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNullConditionalIndexAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        const std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> createOrdinaryAssignmentNode(
        const ast::AstNode* expression,
        const std::string& resultType);
    std::unique_ptr<ProjectOopHirNode> lowerCompoundAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const std::string& resultType,
        std::unique_ptr<ProjectOopHirNode> node,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerSimpleAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::unique_ptr<ProjectOopHirNode> node,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerMoveExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerResultPropagationExpressionImplCase(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const std::string& resultType,
        std::string& error);

    bool hasValidDelegateCreationTarget(
        const MethodDelegateBinding& binding) const noexcept;
    std::unique_ptr<ProjectOopHirNode> finishDelegateCreation(
        const ast::AstNode* expression,
        const MethodDelegateBinding& binding,
        std::unique_ptr<ProjectOopHirNode> receiver);

    std::unique_ptr<ProjectOopHirNode> buildBuiltinBinaryExpression(
        const ast::AstNode* expression,
        std::unique_ptr<ProjectOopHirNode> left,
        std::unique_ptr<ProjectOopHirNode> right);
    bool isEnumBinaryTypeKey(std::string_view typeKey) const noexcept;
    static bool isEnumTypeWithIdentity(
        std::string_view identity,
        const NamedTypeDeclaration& type) noexcept;
    static bool isZeroIntegerLiteral(const ast::AstNode* syntax) noexcept;
    void convertBinaryEnumZeroOperands(
        const ast::AstNode* expression,
        std::unique_ptr<ProjectOopHirNode>& left,
        std::unique_ptr<ProjectOopHirNode>& right);
    std::optional<std::string> numericBinaryOperandType(
        const ast::AstNode* expression,
        const ProjectOopHirNode& left,
        const ProjectOopHirNode& right,
        const std::string& resultType) const;
    void convertNumericBinaryOperands(
        const ast::AstNode* expression,
        const std::string& numericOperandType,
        std::unique_ptr<ProjectOopHirNode>& left,
        std::unique_ptr<ProjectOopHirNode>& right);

    std::unique_ptr<ProjectOopHirNode> buildConditionalExpression(
        const ast::AstNode* expression,
        std::unique_ptr<ProjectOopHirNode> condition,
        std::unique_ptr<ProjectOopHirNode> whenTrue,
        std::unique_ptr<ProjectOopHirNode> whenFalse);
    void convertConditionalBranch(
        std::unique_ptr<ProjectOopHirNode>& branch,
        const ast::AstNode* syntax,
        const std::string& resultType);

    struct BoundCallTarget;

    // Resolve the receiver that lowerBoundCall will actually consume. Keeping this decision in one
    // helper lets an iterative caller lower that exact receiver before it resumes the call frame.
    BoundCallTarget resolveBoundCallTarget(const MethodCallBinding& binding) const noexcept;

    std::unique_ptr<ProjectOopHirNode> lowerBoundCall(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error,
        const MethodExpressionTypeBinding* typeBinding,
        const MethodCallBinding* binding,
        const std::vector<const ast::AstNode*>& argumentSyntaxes,
        std::unique_ptr<ProjectOopHirNode> preloweredLast = nullptr,
        bool nullConditional = false,
        std::unique_ptr<ProjectOopHirNode> preloweredReceiver = nullptr,
        std::vector<std::unique_ptr<ProjectOopHirNode>>* preloweredArguments = nullptr);
    bool boundCallArgumentNeedsAddress(
        std::size_t selectedMemberOrdinal,
        std::size_t argumentIndex) const noexcept;
    std::unique_ptr<ProjectOopHirNode> prepareBoundCallArgument(
        const MethodCallBinding& binding,
        std::size_t selectedMemberOrdinal,
        std::size_t argumentIndex,
        const ast::AstNode* argumentSyntax,
        std::unique_ptr<ProjectOopHirNode> argument,
        bool& hasOwnedTemporary);
    std::unique_ptr<ProjectOopHirNode> finishBoundCall(
        const ast::AstNode* expression,
        bool addressContext,
        std::string& error,
        const MethodExpressionTypeBinding* typeBinding,
        const MethodCallBinding& binding,
        const BoundCallTarget& target,
        std::size_t explicitArgumentCount,
        bool nullConditional,
        std::unique_ptr<ProjectOopHirNode> receiver,
        std::vector<std::unique_ptr<ProjectOopHirNode>> arguments,
        bool hasOwnedTemporary);

    // Completes a namespace call after the common expression worklist has lowered its forwarded
    // receiver and explicit arguments. This finalizer never descends into expression syntax.
    std::unique_ptr<ProjectOopHirNode> finishNamespaceCall(
        const ast::AstNode* expression,
        bool addressContext,
        std::string& error,
        const NamespaceFunctionCallBinding& binding,
        const NamespaceFunctionSymbol& target,
        std::unique_ptr<ProjectOopHirNode> node,
        bool hasOwnedTemporary);

    // Appends delegate/lambda defaults after the common expression worklist has lowered the
    // optional callee and every explicit argument. It never descends into ordinary expressions.
    std::unique_ptr<ProjectOopHirNode> finishDelegateInvoke(
        const ast::AstNode* expression,
        std::string& error,
        const MethodDelegateBinding& binding,
        std::unique_ptr<ProjectOopHirNode> node);

    bool validateNoHeapDirectCall(
        std::size_t callerMemberOrdinal,
        const MethodCallBinding& binding,
        std::string& error) const;
    // Completes a getter whose only recursive child (the receiver) was already lowered by the
    // expression worklist. All other call metadata and diagnostics remain in lowerBoundCall.
    std::unique_ptr<ProjectOopHirNode> lowerPropertyGetterWithReceiver(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error,
        const MethodCallBinding& binding,
        bool nullConditional,
        bool flowProvenNonNull,
        std::unique_ptr<ProjectOopHirNode> receiver);

    // Completes an indexer getter after the worklist has visited its receiver and every explicit
    // index in source order. lowerBoundCall remains the single owner of conversions and borrows.
    std::unique_ptr<ProjectOopHirNode> lowerIndexerGetterWithOperands(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error,
        const MethodCallBinding& binding,
        const std::vector<const ast::AstNode*>& argumentSyntaxes,
        bool nullConditional,
        std::unique_ptr<ProjectOopHirNode> receiver,
        std::vector<std::unique_ptr<ProjectOopHirNode>> arguments);

    // Describes the forwarded receiver contract before the worklist descends into it, then
    // completes the historical namespace-call HIR from the already-lowered receiver.
    std::optional<bool> extensionPropertyReceiverAddress(
        const NamespaceFunctionCallBinding& binding) const noexcept;
    std::unique_ptr<ProjectOopHirNode> lowerExtensionPropertyGetterWithReceiver(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::string& error,
        const NamespaceFunctionCallBinding& binding,
        std::unique_ptr<ProjectOopHirNode> receiver = nullptr);

    std::unique_ptr<ProjectOopHirNode> lowerEventBackingField(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::string& error,
        const MethodEventBinding* binding,
        bool address,
        std::unique_ptr<ProjectOopHirNode> preloweredReceiver = nullptr);

    std::unique_ptr<ProjectOopHirNode> lowerCallExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerAssertionCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerIteratorCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNamespaceFunctionCall(
        const ast::AstNode* expression,
        bool addressContext,
        const NamespaceFunctionCallBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerEventInvocation(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        const MethodEventBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDelegateInvocation(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodDelegateBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerViewCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerOperandCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::size_t operandCount,
        ProjectOopHirNodeKind hirKind,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerAtomicFenceCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> rejectEscapedAtomicCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> rejectEscapedMonitorCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerConsoleCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerEnumToStringCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerStructCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerClockCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerEnvironmentQueryCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerProcessQueryCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerSystemInfoCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDebugCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerEnvironmentExitCoreCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCoreBinding& binding,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerOrdinaryCall(
        const ast::AstNode* expression,
        bool addressContext,
        const MethodCallBinding& binding,
        std::string& error);
    bool appendOrdinaryDefaultArguments(
        ProjectOopHirNode& node,
        const ast::AstNode* expression,
        const MethodCallBinding& binding,
        const ClassMemberSymbol& selectedMember,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishOrdinaryCall(
        std::unique_ptr<ProjectOopHirNode> node,
        const ast::AstNode* expression,
        bool addressContext,
        const ClassMemberSymbol& selectedMember,
        bool hasOwnedTemporary,
        std::string& error);

    std::unique_ptr<ProjectOopHirNode> lowerConversionExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error,
        const MethodExpressionTypeBinding* typeBinding,
        std::string_view resultType);

    std::unique_ptr<ProjectOopHirNode> lowerArrayConstructionExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerStackAllocationExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerNewArrayConstructionExpression(
        const ast::AstNode* expression,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishCollectionBuilderExpression(
        const ast::AstNode* expression,
        const MethodCoreBinding& core,
        std::size_t temporaryOrdinal,
        std::vector<std::unique_ptr<ProjectOopHirNode>> elements);

    std::unique_ptr<ProjectOopHirNode> lowerObjectConstructionExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> finishAllocatorConstructionExpression(
        const ast::AstNode* expression,
        const ObjectConstructionBinding& binding,
        std::size_t constructionOrdinal,
        std::size_t computedSize,
        std::size_t temporaryOrdinal,
        std::size_t contextOrdinal,
        std::optional<std::size_t> allocatorTypeOrdinal,
        std::unique_ptr<ProjectOopHirNode> receiver,
        std::vector<std::unique_ptr<ProjectOopHirNode>> arguments,
        std::vector<std::size_t> argumentParameterOrdinals,
        std::string& error);
    void appendAllocatorPreamble(
        ProjectOopHirNode& block,
        const ast::AstNode* expression,
        const ObjectConstructionBinding& binding,
        std::size_t computedSize,
        std::size_t temporaryOrdinal,
        std::size_t contextOrdinal,
        std::unique_ptr<ProjectOopHirNode> receiver);
    std::unique_ptr<ProjectOopHirNode> createAllocatorPlacementConstruction(
        const ast::AstNode* expression,
        const ObjectConstructionBinding& binding,
        std::size_t constructionOrdinal,
        std::size_t temporaryOrdinal,
        std::size_t contextOrdinal,
        std::optional<std::size_t> allocatorTypeOrdinal,
        std::vector<std::unique_ptr<ProjectOopHirNode>> arguments,
        std::vector<std::size_t> argumentParameterOrdinals);
    std::unique_ptr<ProjectOopHirNode> createAllocatorThrowsFailure(
        const ast::AstNode* expression,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> createAllocatorResultFailure(
        const ast::AstNode* expression,
        const ObjectConstructionBinding& binding,
        std::unique_ptr<ProjectOopHirNode>& construction,
        std::string& resultType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> createAllocatorFailureValue(
        const ast::AstNode* expression,
        const ObjectConstructionBinding& binding,
        std::unique_ptr<ProjectOopHirNode>& construction,
        std::string& resultType,
        std::string& error);

    std::unique_ptr<ProjectOopHirNode> lowerAggregateOrLayoutExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::string& error);

    std::unique_ptr<ProjectOopHirNode> lowerAwaitExpression(
        const ast::AstNode* expression,
        std::unique_ptr<ProjectOopHirNode> operand,
        std::optional<std::size_t> selectedMemberOrdinal,
        std::vector<std::size_t> cleanupRegionOrdinals,
        std::optional<std::size_t> exceptionTargetRegionOrdinal,
        std::string& error);
    bool awaitProducerCanSuspend(std::size_t rootMemberOrdinal) const;

    std::unique_ptr<ProjectOopHirNode> lowerCoroutineStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerCoroutineValueStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);

    // NS304-T05-F03: rewrites every Return node of an async poll body into
    // `self->result = v; self->state = ready; return;`. self is the parameter at
    // `selfParameterOrdinal` (type future:<T>*); resultTypeKey is the awaited inner type T.
    std::unique_ptr<ProjectOopHirNode> rewriteAsyncPollReturns(
        std::unique_ptr<ProjectOopHirNode> body,
        std::size_t selfParameterOrdinal,
        std::string_view resultTypeKey,
        std::string_view futureKey,
        std::string& error,
        bool topLevel = true);
    std::unique_ptr<ProjectOopHirNode> makeAsyncSelfParameter(
        const ast::AstNode* syntax,
        std::size_t selfParameterOrdinal,
        std::string_view futureKey);
    std::unique_ptr<ProjectOopHirNode> rewriteAsyncThrow(
        std::unique_ptr<ProjectOopHirNode> throwing,
        std::size_t selfParameterOrdinal,
        std::string_view futureKey);
    std::unique_ptr<ProjectOopHirNode> rewriteAsyncReturn(
        std::unique_ptr<ProjectOopHirNode> ret,
        std::size_t selfParameterOrdinal,
        std::string_view resultTypeKey,
        std::string_view futureKey);
    static std::unique_ptr<ProjectOopHirNode> rewriteAsyncYield(
        std::unique_ptr<ProjectOopHirNode> yielding);
    void appendImplicitAsyncReturn(
        ProjectOopHirNode& body,
        std::size_t selfParameterOrdinal,
        std::string_view resultTypeKey,
        std::string_view futureKey);
    std::vector<std::size_t> collectAwaitedMemberOrdinals(const ast::AstNode* root) const;
    void assignYieldStates(ProjectOopHirNode* root, std::size_t& nextYieldState);
    void rewriteGeneratorThrows(
        ProjectOopHirNode* root,
        std::string_view errorTypeKey,
        std::string_view iteratorKey);
    std::unique_ptr<ProjectOopHirNode> rewriteGeneratorThrow(
        std::unique_ptr<ProjectOopHirNode> throwing,
        std::string_view errorTypeKey,
        std::string_view iteratorKey);

    // NS304-T05-F03: splits an async function into a lazy constructor (function.body becomes
    // `return default(future:<T>)`) and a poll function (returned; runs the body and stores the
    // result). Returns nullptr and sets error on failure.
    std::unique_ptr<ProjectOopHirFunction> splitAsyncFunction(
        ProjectOopHirFunction& function,
        std::string& error);
    std::unique_ptr<ProjectOopHirFunction> createAsyncPollFunction(
        ProjectOopHirFunction& function,
        const std::string& futureKey,
        const std::string& resultTypeKey,
        std::string& error);
    void buildAsyncConstructorBody(
        ProjectOopHirFunction& function,
        const std::string& futureKey,
        const ast::AstNode* syntax);

    // NS304-T06-F03: splits a generator into a lazy constructor (function.body becomes
    // `return default(iterator:<T>)`) and a MoveNext function (returned; `bool(self)` runs the body
    // to the next yield). NS304-T06-F07: also returns a Drop function (`void(self)` destroys the
    // frame's live locals). Returns {nullptr, nullptr} and sets error on failure.
    std::pair<std::unique_ptr<ProjectOopHirFunction>, std::unique_ptr<ProjectOopHirFunction>>
    splitGeneratorFunction(
        ProjectOopHirFunction& function,
        std::string& error);
    std::unique_ptr<ProjectOopHirFunction> createGeneratorMoveNext(
        ProjectOopHirFunction& function,
        const std::string& iteratorKey,
        const ast::AstNode* syntax);
    void buildGeneratorConstructorBody(
        ProjectOopHirFunction& function,
        const std::string& iteratorKey,
        const ast::AstNode* syntax);
    std::unique_ptr<ProjectOopHirFunction> createGeneratorDrop(
        const ProjectOopHirFunction& function,
        const std::string& iteratorKey,
        const ast::AstNode* syntax);

    // NS304-T07-F02: splits an async generator into (in order) the MoveNext (future<bool>), its
    // poll (void) and the drop (void). The original function becomes the constructor
    // (async-iterator + moveNextFn + dropFn). Returns empty + error on failure.
    std::vector<std::unique_ptr<ProjectOopHirFunction>> splitAsyncGeneratorFunction(
        ProjectOopHirFunction& function,
        std::string& error);
    void wrapAsyncGeneratorBody(
        ProjectOopHirFunction& function,
        const ast::AstNode* syntax);
    std::unique_ptr<ProjectOopHirFunction> createAsyncGeneratorPoll(
        ProjectOopHirFunction& function,
        const std::string& futureKey,
        const std::string& errorTypeKey,
        std::string& error);
    std::unique_ptr<ProjectOopHirFunction> createAsyncGeneratorMoveNext(
        const ProjectOopHirFunction& function,
        const std::string& iteratorKey,
        const std::string& futureKey,
        const std::string& errorTypeKey,
        const ast::AstNode* syntax);
    std::unique_ptr<ProjectOopHirFunction> createAsyncGeneratorDrop(
        const ProjectOopHirFunction& function,
        const std::string& iteratorKey,
        const ast::AstNode* syntax);
    void buildAsyncGeneratorConstructorBody(
        ProjectOopHirFunction& function,
        const std::string& iteratorKey,
        const ast::AstNode* syntax);

public:
    // El rango del nodo mas interno que no se pudo bajar, o un rango invalido si el fallo no vino
    // de una expresion. Quien reporta el error lo traduce a fichero:linea:columna con el
    // SourceManager -- este modelo no lo tiene y no le hace falta para recordar el sitio.
    SourceRange lastErrorRange() const noexcept;

private:
    SourceRange lastErrorRange_{};
    // C6 "struct members": true when the expression denotes storage that already exists, so a
    // struct receiver can take its address directly instead of needing a temporary.
    bool isNaturallyAddressable(const ast::AstNode* expression) const noexcept;
    // NSC-LIMIT-008. The call binding of a name that the semantic model resolved to a
    // PROPERTY/INDEXER GETTER -- a call wearing the clothes of a name -- or null when the name is
    // real storage (a field, an array element) or is not resolved at all.
    //
    // It returns the BINDING and not a bool because the two callers need different halves of the
    // same answer: lowerExpression needs the binding to lower the accessor call, and
    // isNaturallyAddressable only needs to know whether there was one. Returning a bool would leave
    // lowerExpression re-deriving the binding, which is how the two sites came to disagree in the
    // first place.
    const MethodCallBinding* accessorGetterBinding(const ast::AstNode* expression) const noexcept;
    std::unique_ptr<ProjectOopHirNode> lowerImplicitReceiver(
        const ast::AstNode* receiverSyntax,
        std::string_view receiverTypeKey,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerReceiver(
        const ast::AstNode* receiverSyntax,
        std::string_view receiverTypeKey,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDeconstructionSource(
        const ast::AstNode* bindingSyntax,
        const ast::AstNode* sourceSyntax,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> wrapImplicitConversion(
        std::unique_ptr<ProjectOopHirNode> value,
        const ProjectImplicitConversion& conversion,
        const ast::AstNode* syntax);
    std::unique_ptr<ProjectOopHirNode> wrapCallArgumentBorrow(
        std::unique_ptr<ProjectOopHirNode> value,
        const ast::AstNode* argumentSyntax,
        std::size_t targetMemberOrdinal,
        std::size_t targetParameterOrdinal,
        bool& hasOwnedTemporary);
    std::unique_ptr<ProjectOopHirNode> wrapFreshOwnedTemporaryBorrow(
        std::unique_ptr<ProjectOopHirNode> value,
        const ast::AstNode* argumentSyntax,
        bool& hasOwnedTemporary);
    std::unique_ptr<ProjectOopHirNode> wrapFullExpressionCleanup(
        std::unique_ptr<ProjectOopHirNode> value,
        const ast::AstNode* syntax,
        bool hasOwnedTemporary);
    void applyStructuredExitBinding(
        ProjectOopHirNode& node,
        const ast::AstNode* syntax) const;
    std::optional<std::size_t> cleanupRegionOrdinal(
        const MethodCleanupRegion* region) const;
    std::unique_ptr<ProjectOopHirNode> lowerExceptionOrExitStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDestructionExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDeleteExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerArrayDeleteExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        const MethodExpressionTypeBinding& operandType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerScalarDeleteExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        const MethodExpressionTypeBinding& operandType,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDestroyExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerDefaultArgument(
        const ast::AstNode* syntax,
        std::string_view parameterTypeKey,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerInstanceFieldInitialization(
        std::size_t fieldMemberOrdinal,
        std::optional<std::size_t> callerMemberOrdinal,
        std::string& error);
    void appendInstanceFieldInitializers(
        std::size_t typeOrdinal,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& output,
        std::string& error,
        std::optional<std::size_t> callerMemberOrdinal = std::nullopt);
    std::vector<std::size_t> implicitInitializerTypes(
        std::size_t targetTypeOrdinal,
        std::optional<std::size_t> entryConstructorMemberOrdinal) const;

    const MethodNameBinding* nameBinding(const ast::AstNode* syntax) const noexcept;
    const MethodExpressionTypeBinding* expressionType(const ast::AstNode* syntax) const noexcept;
    const MethodCallBinding* callBinding(const ast::AstNode* syntax) const noexcept;
    const NamespaceFunctionCallBinding* namespaceFunctionCallBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodSwitchBinding* switchBinding(const ast::AstNode* syntax) const noexcept;
    const MethodRuntimeTypeBinding* runtimeTypeBinding(const ast::AstNode* syntax) const noexcept;
    const MethodDelegateBinding* delegateBinding(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaBinding* lambdaBinding(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaValueBinding* lambdaValueBinding(const ast::AstNode* syntax) const noexcept;
    std::optional<std::size_t> currentLambdaThisCapture() const noexcept;
    const MethodEventBinding* eventBinding(const ast::AstNode* syntax) const noexcept;
    const MethodCoreBinding* coreBinding(const ast::AstNode* syntax) const noexcept;
    const MethodOptionalCarrierBinding* optionalCarrierBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodResultPropagationBinding* resultPropagationBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodRangeSliceBinding* rangeSliceBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodExceptionBinding* exceptionBinding(const ast::AstNode* syntax) const noexcept;
    const MethodCleanupRegion* cleanupRegion(const ast::AstNode* syntax) const noexcept;
    const MethodUsingBinding* usingBinding(const ast::AstNode* syntax) const noexcept;
    const MethodMoveBinding* moveBinding(const ast::AstNode* syntax) const noexcept;
    const MethodBorrowBinding* borrowBinding(const ast::AstNode* syntax) const noexcept;
    const MethodStructuredExitBinding* structuredExitBinding(const ast::AstNode* syntax) const noexcept;
    const ObjectConstructionBinding* constructionBinding(const ast::AstNode* syntax) const noexcept;
    const MethodLocalSymbol* localSymbol(const ast::AstNode* syntax) const noexcept;
    ProjectImplicitConversion classifyImplicit(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        std::optional<ProjectIntegerConstant> integerConstant = std::nullopt) const;
    bool isStructValueType(std::string_view typeKey) const noexcept;
    // C8 "type root", struct half. See the definitions for the contract the leaves define.
    std::string shortTypeName(std::string_view typeKey) const;
    bool appendAggregateLeaves(
        const std::string& typeKey,
        std::uint64_t baseOffset,
        const ast::AstNode* syntax,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& leaves,
        std::string& error);
    // C7 "aggregate initializer": flattens one braced initializer -- and, recursively, any braced
    // initializer nested inside it -- into AggregateElement nodes carrying absolute byte offsets
    // measured from the start of the OUTERMOST aggregate. `baseOffset` is where the aggregate
    // being filled starts within that outermost one.
    bool appendAggregateElements(
        const ast::AstNode* syntax,
        const std::string& targetTypeKey,
        std::uint64_t baseOffset,
        std::size_t memberOrdinal,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& elements,
        std::string& error);
    bool initializeAggregateFrame(internal::HirAggregateFrame& frame, std::string& error);
    bool populateTupleAggregateSlots(internal::HirAggregateFrame& frame, std::string& error);
    bool populateFixedArrayAggregateSlots(
        internal::HirAggregateFrame& frame,
        const std::string& elementTypeKey,
        std::string& error);
    bool populateStructAggregateSlots(internal::HirAggregateFrame& frame, std::string& error);
    bool processAggregateFrame(
        std::vector<internal::HirAggregateFrame>& pending,
        std::string& error);
    bool isClassReferenceType(std::string_view typeKey) const noexcept;
    bool isReferenceType(std::string_view typeKey) const noexcept;
    // NS304-T08-F04: resolves a member ordinal to a class member or a namespace-callable adapter.
    const ClassMemberSymbol* callableSymbol(std::size_t ordinal) const noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectMethodBodyModel* methodBodyModel_ = nullptr;
    const ProjectNamespaceFunctionModel* namespaceFunctionModel_ = nullptr;
    const ProjectConstructionModel* constructionModel_ = nullptr;
    const ProjectObjectLayoutModel* objectLayoutModel_ = nullptr;
    const ProjectGenericConstraintModel* genericConstraintModel_ = nullptr;
    const ProjectMetaprogrammingModel* metaprogrammingModel_ = nullptr;
    const ProjectOopHirModel* lookupSource_ = nullptr;
    ProjectCompilationIndexes* indexes_ = nullptr;
    driver::CompilationWorkerPool* workerPool_ = nullptr;
    std::vector<ProjectOopHirFunction> functions_;
    std::vector<ProjectOopHirFieldInitializer> fieldInitializers_;
    std::vector<std::optional<std::size_t>> functionByMember_;
    std::vector<std::optional<std::size_t>> functionByNamespaceFunction_;
    std::unordered_map<const ast::AstNode*, const MethodNameBinding*> nameBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodExpressionTypeBinding*> expressionTypeBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCallBinding*> callBySyntax_;
    std::unordered_map<const ast::AstNode*, const NamespaceFunctionCallBinding*>
        namespaceFunctionCallBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodSwitchBinding*> switchBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodRuntimeTypeBinding*> runtimeTypeBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodDelegateBinding*> delegateBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaBinding*> lambdaBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaValueBinding*> lambdaValueBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodEventBinding*> eventBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCoreBinding*> coreBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodOptionalCarrierBinding*>
        optionalCarrierBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodResultPropagationBinding*>
        resultPropagationBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodRangeSliceBinding*>
        rangeSliceBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodExceptionBinding*> exceptionBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCleanupRegion*> cleanupRegionBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodUsingBinding*> usingBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodMoveBinding*> moveBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodBorrowBinding*> borrowBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodStructuredExitBinding*> structuredExitBySyntax_;
    std::unordered_map<const ast::AstNode*, const ObjectConstructionBinding*> constructionBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLocalSymbol*> localBySyntax_;
    struct HirExceptionHandlerContext;

    const MethodLambdaBinding* currentLambda_ = nullptr;
    std::vector<std::size_t> activeHirCleanupRegions_;
    std::vector<HirExceptionHandlerContext> activeHirExceptionHandlers_;
    std::size_t nextHirRegionOrdinal_ = 0;
    std::size_t nextSyntheticLocalOrdinal_ = 0;
    std::size_t nextSyntheticCleanupRegionOrdinal_ = 0;
    bool loweringAsyncBody_ = false;
    bool built_ = false;
};

const char* projectOopHirNodeKindName(ProjectOopHirNodeKind kind) noexcept;
}

#include "nativesharp/Frontend/ProjectOopHirModelBoundCallTarget.h"
#include "nativesharp/Frontend/ProjectOopHirModelHirExceptionHandlerContext.h"
