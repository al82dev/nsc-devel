#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Language/CoroutineSemantics.h"

#include <limits>
#include <optional>
#include <string_view>

namespace nativesharp::frontend
{
using internal::CoroutineValueStatementFrameState;
using internal::ExitValueStatementFrameState;
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::TryStatementFrameState;
using internal::UsingStatementFrameState;

// Aplica los contratos de efecto al resultado de una frontera de expresion.
// Applies effect contracts to an expression-boundary result.
bool ProjectOopHirModel::applyExpressionBoundaryPostconditions(
    std::unique_ptr<ProjectOopHirNode>& child,
    const ast::AstNode* childSyntax,
    std::size_t childMemberOrdinal,
    [[maybe_unused]] std::string& error)
{
    const ClassMemberSymbol* activeMember = memberModel_ != nullptr &&
            childMemberOrdinal < memberModel_->members().size()
        ? &memberModel_->members()[childMemberOrdinal]
        : nullptr;
    const sema::EffectContext effects(activeMember != nullptr
        ? activeMember->effects
        : sema::EffectSet{});
    if (child != nullptr && effects.allows(sema::Effect::NoHeap))
    {
        rejectDirectNoHeapAllocation(child, *activeMember, error);
        if (child != nullptr)
            rejectNoHeapBoundCall(child, childSyntax, *activeMember, error);
        if (child != nullptr)
            rejectNoHeapIndirectInvocation(child, *activeMember, error);
        if (child != nullptr)
            rejectNoHeapConstruction(child, childSyntax, *activeMember, error);
    }
    if (child == nullptr && !error.empty() &&
        !lastErrorRange_.isValid() && childSyntax != nullptr)
    {
        lastErrorRange_ = childSyntax->range();
    }
    return child != nullptr;
}

const ProjectOopHirNode* ProjectOopHirModel::rootLoweredExpression(
    [[maybe_unused]] const std::unique_ptr<ProjectOopHirNode>& lowered,
    const ast::AstNode* statementContinuation) noexcept
{
    const ProjectOopHirNode* expression = lowered.get();
    if (statementContinuation != nullptr &&
        statementContinuation->kind() == ast::AstNodeKind::ExpressionStatement &&
        expression != nullptr &&
        expression->kind == ProjectOopHirNodeKind::ExpressionStatement &&
        expression->children.size() == 1U)
    {
        return expression->children.front().get();
    }
    return expression;
}

bool ProjectOopHirModel::statementChildPolicyApplied(
    const ast::AstNode* statementContinuation) noexcept
{
    if (statementContinuation == nullptr) return false;
    switch (statementContinuation->kind())
    {
        case ast::AstNodeKind::StaticAssertStatement:
        case ast::AstNodeKind::IfStatement:
        case ast::AstNodeKind::WhileStatement:
        case ast::AstNodeKind::DoWhileStatement:
        case ast::AstNodeKind::SwitchStatement:
        case ast::AstNodeKind::ForStatement:
        case ast::AstNodeKind::ForeachStatement:
        case ast::AstNodeKind::UsingStatement:
        case ast::AstNodeKind::TryStatement:
        case ast::AstNodeKind::YieldReturnStatement:
        case ast::AstNodeKind::BreakStatement:
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::ThrowStatement:
            return true;
        default:
            return false;
    }
}

void ProjectOopHirModel::applyRootExpressionPostconditions(
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const ast::AstNode* statementContinuation,
    [[maybe_unused]] std::string& error)
{
    const ClassMemberSymbol* activeMember = memberModel_ != nullptr &&
            memberOrdinal < memberModel_->members().size()
        ? &memberModel_->members()[memberOrdinal]
        : nullptr;
    const sema::EffectContext effects(activeMember != nullptr
        ? activeMember->effects
        : sema::EffectSet{});
    if (!statementChildPolicyApplied(statementContinuation) &&
        lowered != nullptr && effects.allows(sema::Effect::NoHeap))
    {
        const ProjectOopHirNode* loweredExpression =
            rootLoweredExpression(lowered, statementContinuation);
        if (loweredExpression != nullptr &&
            directlyAllocatesHeap(loweredExpression->kind))
        {
            error = "NS3030: noheap callable '" + activeMember->name +
                "' cannot contain direct heap allocation '" +
                std::string(projectOopHirNodeKindName(loweredExpression->kind)) +
                "'";
            lowered.reset();
        }
        if (lowered != nullptr)
            rejectNoHeapBoundCall(lowered, expression, *activeMember, error);
        if (lowered != nullptr)
            rejectNoHeapIndirectInvocation(lowered, *activeMember, error);
        if (lowered != nullptr)
            rejectNoHeapConstruction(lowered, expression, *activeMember, error);
    }
    // Solo se apunta si NO hay nada apuntado: asi gana el nodo mas interno, que es el que de
    // verdad fallo, y no la expresion que lo contiene.
    if (lowered == nullptr && !error.empty() &&
        !lastErrorRange_.isValid() && expression != nullptr)
    {
        lastErrorRange_ = expression->range();
    }
}

bool ProjectOopHirModel::directlyAllocatesHeap(
    ProjectOopHirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopHirNodeKind::ArrayAllocation:
        case ProjectOopHirNodeKind::RangeArrayAllocation:
        case ProjectOopHirNodeKind::RawAllocation:
        case ProjectOopHirNodeKind::StringConcat:
        case ProjectOopHirNodeKind::StringSubstring:
        case ProjectOopHirNodeKind::StringFromCharArray:
        case ProjectOopHirNodeKind::DelegateCreate:
        case ProjectOopHirNodeKind::LambdaCreate:
        case ProjectOopHirNodeKind::EventAdd:
            return true;
        default:
            return false;
    }
}

void ProjectOopHirModel::rejectDirectNoHeapAllocation(
    std::unique_ptr<ProjectOopHirNode>& child,
    const ClassMemberSymbol& activeMember,
    [[maybe_unused]] std::string& error)
{
    const ProjectOopHirNode* loweredChild = child.get();
    if (loweredChild == nullptr || !directlyAllocatesHeap(loweredChild->kind)) return;
    error = "NS3030: noheap callable '" + activeMember.name +
        "' cannot contain direct heap allocation '" +
        std::string(projectOopHirNodeKindName(loweredChild->kind)) + "'";
    child.reset();
}

void ProjectOopHirModel::rejectNoHeapBoundCall(
    std::unique_ptr<ProjectOopHirNode>& child,
    const ast::AstNode* childSyntax,
    const ClassMemberSymbol& activeMember,
    [[maybe_unused]] std::string& error)
{
    const MethodCallBinding* call = callBinding(childSyntax);
    if (call == nullptr ||
        call->resolution != MethodCallResolutionKind::Resolved ||
        !call->selectedMemberOrdinal.has_value() ||
        *call->selectedMemberOrdinal >= memberModel_->members().size())
    {
        return;
    }
    const ClassMemberSymbol& callee =
        memberModel_->members()[*call->selectedMemberOrdinal];
    if (callee.isNoHeap) return;
    error = "NS3030: noheap callable '" + activeMember.signatureKey +
        "' cannot call non-noheap callable '" + callee.signatureKey + "'";
    child.reset();
}

void ProjectOopHirModel::rejectNoHeapIndirectInvocation(
    std::unique_ptr<ProjectOopHirNode>& child,
    const ClassMemberSymbol& activeMember,
    [[maybe_unused]] std::string& error)
{
    const ProjectOopHirNode* loweredChild = child.get();
    if (loweredChild == nullptr ||
        (loweredChild->kind != ProjectOopHirNodeKind::DelegateInvoke &&
         loweredChild->kind != ProjectOopHirNodeKind::EventInvoke))
    {
        return;
    }
    error = "NS3030: noheap callable '" + activeMember.signatureKey +
        "' cannot invoke an indirect target without a noheap contract";
    child.reset();
}

void ProjectOopHirModel::rejectNoHeapConstruction(
    std::unique_ptr<ProjectOopHirNode>& child,
    const ast::AstNode* childSyntax,
    const ClassMemberSymbol& activeMember,
    [[maybe_unused]] std::string& error)
{
    const ProjectOopHirNode* loweredChild = child.get();
    if (loweredChild == nullptr ||
        (loweredChild->kind != ProjectOopHirNodeKind::PlacementConstruction &&
         loweredChild->kind != ProjectOopHirNodeKind::ValueConstruction))
    {
        return;
    }
    const ObjectConstructionBinding* binding = constructionBinding(childSyntax);
    if (binding == nullptr ||
        !binding->selectedConstructorMemberOrdinal.has_value() ||
        *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
    {
        error = "NS3030: noheap callable '" + activeMember.signatureKey +
            "' cannot invoke an implicit constructor without a noheap contract";
        child.reset();
        return;
    }
    const ClassMemberSymbol& constructor =
        memberModel_->members()[*binding->selectedConstructorMemberOrdinal];
    if (constructor.isNoHeap) return;
    error = "NS3030: noheap callable '" + activeMember.signatureKey +
        "' cannot call non-noheap constructor '" + constructor.signatureKey + "'";
    child.reset();
}

// Inicializa el frame de switch con su binding semantico congelado.
// Initializes the switch frame with its frozen semantic binding.
bool ProjectOopHirModel::initializeSwitchStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::string& error)
{
    const MethodSwitchBinding* binding = switchBinding(statement);
    if (binding == nullptr)
    {
        error = "Stage 34 OOP HIR reached a switch statement with no semantic binding";
        return false;
    }
    frame.kind = IterativeExpressionKind::SwitchStatement;
    frame.syntax = statement;
    frame.switchStatement.emplace();
    frame.switchStatement->binding = binding;
    frame.switchStatement->node = makeNode(
        ProjectOopHirNodeKind::Switch, statement, "builtin:void");
    return true;
}

void ProjectOopHirModel::initializeForStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement)
{
    frame.kind = IterativeExpressionKind::ForStatement;
    frame.syntax = statement;
    frame.forStatement.emplace();
    frame.forStatement->node = makeNode(
        ProjectOopHirNodeKind::For, statement, "builtin:void");
    frame.cleanupDepth = activeHirCleanupRegions_.size();
    frame.cleanupActive = true;
}

bool ProjectOopHirModel::initializeForeachStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::string& error)
{
    const MethodLocalSymbol* sourceLocal = localSymbol(statement->first());
    if (sourceLocal == nullptr)
    {
        error = "Stage 34 foreach source local is missing its Stage 27 semantic binding";
        return false;
    }
    const bool isArray = sourceLocal->typeKey.size() > 2U &&
        sourceLocal->typeKey.compare(
            sourceLocal->typeKey.size() - 2U, 2U, "[]") == 0;
    const bool isSpan =
        sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span<") ||
        sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span__g") ||
        sourceLocal->typeKey.find("::NativeSharp.Core.Span__g") != std::string::npos ||
        sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan<") ||
        sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan__g") ||
        sourceLocal->typeKey.find("::NativeSharp.Core.ReadOnlySpan__g") != std::string::npos;

    frame.kind = IterativeExpressionKind::ForeachStatement;
    frame.syntax = statement;
    frame.foreachStatement.emplace();
    frame.foreachStatement->node = makeNode(
        ProjectOopHirNodeKind::Block, statement, "builtin:void");
    frame.foreachStatement->chosenCandidate = isArray
        ? statement->second()
        : (isSpan && statement->children().size() > 1U
            ? statement->children()[1]
            : statement->third());
    return true;
}

bool ProjectOopHirModel::initializeExitValueStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::string& error)
{
    frame.kind = IterativeExpressionKind::ExitValueStatement;
    frame.syntax = statement;
    frame.exitValueStatement.emplace();
    switch (statement->kind())
    {
        case ast::AstNodeKind::BreakStatement:
            return initializeBreakValueStatementFrame(frame, statement, error);
        case ast::AstNodeKind::ReturnStatement:
            return initializeReturnValueStatementFrame(frame, statement, error);
        case ast::AstNodeKind::ThrowStatement:
            return initializeThrowValueStatementFrame(frame, statement, error);
        default:
            error = "non-value exit statement reached iterative HIR exit lowering";
            return false;
    }
}

bool ProjectOopHirModel::initializeBreakValueStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::string& error)
{
    ExitValueStatementFrameState& state = *frame.exitValueStatement;
    state.node = makeNode(ProjectOopHirNodeKind::Break, statement, "builtin:void");
    applyStructuredExitBinding(*state.node, statement);
    state.node->stringValue = statement->text();
    state.loopBreakBinding = methodBodyModel_ != nullptr
        ? methodBodyModel_->loopBreakBinding(statement) : nullptr;
    if (state.loopBreakBinding == nullptr)
    {
        error = "T07-F05 break value has no loop-expression semantic binding";
        return false;
    }
    frame.addressContext = false;
    return true;
}

bool ProjectOopHirModel::initializeReturnValueStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::string& error)
{
    ExitValueStatementFrameState& state = *frame.exitValueStatement;
    state.node = makeNode(
        ProjectOopHirNodeKind::Return, statement, "builtin:void");
    applyStructuredExitBinding(*state.node, statement);
    if (memberModel_ != nullptr &&
        frame.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& member =
            memberModel_->members()[frame.memberOrdinal];
        state.referenceReturn = member.isReferenceReturn;
        state.valueReturningCallable = member.kind == ClassMemberKind::Method;
        state.declaredTypeKey = member.declaredTypeKey;
        state.declaredOwnership = member.declaredOwnership;
    }
    else
    {
        const std::size_t callableBase = memberModel_ != nullptr
            ? memberModel_->members().size() : 0U;
        const std::size_t functionOrdinal = frame.memberOrdinal >= callableBase
            ? frame.memberOrdinal - callableBase
            : std::numeric_limits<std::size_t>::max();
        if (namespaceFunctionModel_ == nullptr ||
            functionOrdinal >= namespaceFunctionModel_->functions().size())
        {
            error = "Stage 34 return refers to invalid callable";
            return false;
        }
        const NamespaceFunctionSymbol& function =
            namespaceFunctionModel_->functions()[functionOrdinal];
        state.referenceReturn = function.isReferenceReturn;
        state.valueReturningCallable = true;
        state.declaredTypeKey = function.declaredTypeKey;
        state.declaredOwnership = function.declaredOwnership;
    }
    frame.addressContext = state.referenceReturn;
    return true;
}

bool ProjectOopHirModel::initializeThrowValueStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::string& error)
{
    ExitValueStatementFrameState& state = *frame.exitValueStatement;
    const MethodExceptionBinding* binding = exceptionBinding(statement);
    if (binding == nullptr)
    {
        error = "Stage 45.6 throw statement has no Stage 45.2 exception binding";
        return false;
    }
    state.node = makeNode(
        binding->kind == MethodExceptionOperationKind::Rethrow
            ? ProjectOopHirNodeKind::Rethrow
            : ProjectOopHirNodeKind::Throw,
        statement,
        binding->exceptionTypeKey.empty() ? "builtin:void" : binding->exceptionTypeKey);
    state.node->exceptionTypeOrdinal = binding->exceptionTypeOrdinal;
    state.node->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;

    const std::size_t retainedDepth = activeHirExceptionHandlers_.empty()
        ? 0U
        : activeHirExceptionHandlers_.back().retainedCleanupDepth;
    for (std::size_t index = activeHirCleanupRegions_.size();
        index > retainedDepth; --index)
    {
        state.node->cleanupRegionOrdinals.push_back(
            activeHirCleanupRegions_[index - 1U]);
    }
    if (!activeHirExceptionHandlers_.empty())
    {
        state.node->exceptionTargetRegionOrdinal =
            activeHirExceptionHandlers_.back().hirRegionOrdinal;
    }
    frame.addressContext = false;
    return true;
}

void ProjectOopHirModel::initializeCoroutineValueStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::size_t statementMemberOrdinal)
{
    frame.kind = IterativeExpressionKind::CoroutineValueStatement;
    frame.syntax = statement;
    frame.coroutineValueStatement.emplace();
    CoroutineValueStatementFrameState& state = *frame.coroutineValueStatement;
    state.node = makeNode(
        ProjectOopHirNodeKind::YieldValue, statement, "builtin:void");
    state.node->integerValue = 0U;
    state.node->cleanupRegionOrdinals.assign(
        activeHirCleanupRegions_.begin(), activeHirCleanupRegions_.end());
    state.node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;

    frame.addressContext = false;
    if (memberModel_ == nullptr ||
        statementMemberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& member =
        memberModel_->members()[statementMemberOrdinal];
    std::optional<std::string_view> itemInner =
        language::iteratorInnerTypeKey(member.declaredTypeKey);
    if (!itemInner.has_value())
    {
        itemInner = language::asyncIteratorInnerTypeKey(member.declaredTypeKey);
    }
    frame.addressContext = itemInner.has_value() &&
        (itemInner->starts_with("ref:") || itemInner->starts_with("refmut:"));
}

bool ProjectOopHirModel::initializeUsingStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::string& error)
{
    const MethodUsingBinding* binding = usingBinding(statement);
    const std::optional<std::size_t> cleanupOrdinal =
        cleanupRegionOrdinal(cleanupRegion(statement));
    if (binding == nullptr || !cleanupOrdinal.has_value())
    {
        error = "Stage 45.6 using statement has no frozen using/cleanup binding";
        return false;
    }

    frame.kind = IterativeExpressionKind::UsingStatement;
    frame.syntax = statement;
    frame.usingStatement.emplace();
    UsingStatementFrameState& state = *frame.usingStatement;
    state.binding = binding;
    state.cleanupOrdinal = *cleanupOrdinal;
    state.node = makeNode(
        ProjectOopHirNodeKind::UsingRegion, statement, "builtin:void");
    state.node->localOrdinal = binding->resourceLocalOrdinal;
    state.node->elementTypeKey = binding->resourceTypeKey;
    if (methodBodyModel_ != nullptr &&
        binding->resourceLocalOrdinal < methodBodyModel_->locals().size())
    {
        state.node->stringValue =
            methodBodyModel_->locals()[binding->resourceLocalOrdinal].name;
    }
    state.node->cleanupRegionOrdinal = cleanupOrdinal;
    state.node->disposeContractMemberOrdinal = binding->disposeContractMemberOrdinal;
    state.node->disposeImplementationMemberOrdinal =
        binding->disposeImplementationMemberOrdinal;
    state.node->interfaceCleanupDispatch = binding->interfaceDispatch;
    return true;
}

void ProjectOopHirModel::initializeTryStatementFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement)
{
    frame.kind = IterativeExpressionKind::TryStatement;
    frame.syntax = statement;
    frame.tryStatement.emplace();
    TryStatementFrameState& state = *frame.tryStatement;
    state.node = makeNode(
        ProjectOopHirNodeKind::TryRegion, statement, "builtin:void");
    state.node->hirRegionOrdinal = nextHirRegionOrdinal_++;
    state.cleanupOrdinal = cleanupRegionOrdinal(cleanupRegion(statement));
    frame.cleanupDepth = activeHirCleanupRegions_.size();
    if (state.cleanupOrdinal.has_value())
    {
        state.node->cleanupRegionOrdinal = state.cleanupOrdinal;
        activeHirCleanupRegions_.push_back(*state.cleanupOrdinal);
        frame.cleanupActive = true;
    }
    state.exceptionHandlerDepth = activeHirExceptionHandlers_.size();
    if (!statement->children().empty())
    {
        activeHirExceptionHandlers_.push_back(HirExceptionHandlerContext{
            *state.node->hirRegionOrdinal,
            activeHirCleanupRegions_.size()});
        state.handlerActive = true;
    }
}
}
