#include "HirLoweringSupport.h"
#include "ExpressionStatementMachine/HirExpressionStatementMachineTypes.h"

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionKind;
using internal::BoundCallFrameStage;
using internal::ReceiverFrameState;
using internal::BoundCallFrameState;
using internal::NamespaceCallFrameStage;
using internal::NamespaceCallFrameState;
using internal::EventInvokeFrameStage;
using internal::EventInvokeFrameState;
using internal::DelegateInvokeFrameStage;
using internal::DelegateInvokeFrameState;
using internal::CoreOperandCallFrameState;
using internal::AtomicCoreCallFrameStage;
using internal::AtomicCoreCallFrameState;
using internal::MonitorCoreCallFrameStage;
using internal::MonitorCoreCallFrameState;
using internal::ConsoleOutputCallFrameState;
using internal::EnumToStringCallFrameState;
using internal::StructAggregateCallFrameStage;
using internal::StructAggregateCallFrameState;
using internal::OrdinaryCallFrameStage;
using internal::OrdinaryCallFrameState;
using internal::AllocatorConstructionFrameStage;
using internal::AllocatorConstructionFrameState;
using internal::OrdinaryConstructionFrameStage;
using internal::OrdinaryConstructionFrameState;
using internal::AggregateCrossSlot;
using internal::AggregateCrossStructuralFrame;
using internal::AggregateCrossFramePhase;
using internal::AggregateCrossFrameState;
using internal::VariableDeclarationFrameMode;
using internal::VariableDeclarationFrameState;
using internal::DeconstructionDeclarationFrameStage;
using internal::DeconstructionDeclarationFrameState;
using internal::InlineAssemblyFrameState;
using internal::SwitchStatementFrameState;
using internal::ForStatementFrameState;
using internal::ForeachStatementFrameState;
using internal::ExitValueStatementFrameState;
using internal::CoroutineValueStatementFrameState;
using internal::UsingStatementFrameState;
using internal::TryStatementFrameState;
using internal::IterativeExpressionFrame;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;

namespace
{
bool isComparison(lexer::TokenKind kind) noexcept
{
    return kind == lexer::TokenKind::EqualEqual || kind == lexer::TokenKind::BangEqual ||
        kind == lexer::TokenKind::Less || kind == lexer::TokenKind::LessEqual ||
        kind == lexer::TokenKind::Greater || kind == lexer::TokenKind::GreaterEqual;
}

std::unique_ptr<ProjectOopHirNode> buildInterpolationHandlerExpression(
    const ast::AstNode* expression,
    const MethodCoreBinding& core,
    std::size_t tempOrdinal,
    const std::vector<language::InterpolationPart>& parts,
    std::vector<std::unique_ptr<ProjectOopHirNode>> holes,
    std::string& error)
{
    const std::string& handlerType = core.resultTypeKey;
    auto block = makeNode(ProjectOopHirNodeKind::BlockExpression, expression, handlerType);
    auto declaration = makeNode(
        ProjectOopHirNodeKind::LocalDeclaration, expression, handlerType);
    declaration->localOrdinal = tempOrdinal;
    declaration->stringValue = "handler";
    declaration->children.push_back(makeNode(
        ProjectOopHirNodeKind::DefaultValue, expression, handlerType));
    block->children.push_back(std::move(declaration));

    std::size_t holeIndex = 0;
    for (const language::InterpolationPart& part : parts)
    {
        const bool isHole = part.kind == language::InterpolationPartKind::Hole;
        const std::size_t builderOrdinal =
            isHole ? core.builderMemberOrdinals[1] : core.builderMemberOrdinals[0];
        std::unique_ptr<ProjectOopHirNode> argument;
        if (isHole)
        {
            if (holeIndex >= holes.size())
            {
                error = "interpolation handler hole has no expression";
                return nullptr;
            }
            argument = std::move(holes[holeIndex++]);
        }
        else
        {
            argument = makeNode(
                ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            argument->stringValue = part.text;
        }
        auto call = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, "builtin:void");
        call->memberOrdinal = builderOrdinal;
        auto receiver = makeNode(
            ProjectOopHirNodeKind::LocalAddress, expression, handlerType + "*");
        receiver->localOrdinal = tempOrdinal;
        call->children.push_back(std::move(receiver));
        call->children.push_back(std::move(argument));
        auto statement = makeNode(
            ProjectOopHirNodeKind::ExpressionStatement, expression, "builtin:void");
        statement->children.push_back(std::move(call));
        block->children.push_back(std::move(statement));
    }

    auto finish = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, handlerType);
    finish->memberOrdinal = core.builderMemberOrdinals[2];
    auto finishReceiver = makeNode(
        ProjectOopHirNodeKind::LocalAddress, expression, handlerType + "*");
    finishReceiver->localOrdinal = tempOrdinal;
    finish->children.push_back(std::move(finishReceiver));
    block->children.push_back(std::move(finish));
    return block;
}


}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        expression, memberOrdinal, addressContext, nullptr, error);
}

// Seeds an expression-statement completion frame and returns the completed HIR
// wrapper after the child expression has been lowered.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerVariableDeclarationStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        nullptr, memberOrdinal, false, statement, error);
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::lowerDeconstructionDeclarationStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        nullptr, memberOrdinal, false, statement, error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerInlineAssemblyStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        nullptr, memberOrdinal, false, statement, error);
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::lowerStaticAssertStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerIfStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerLoopStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerSwitchStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerForStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerForeachStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExitValueStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerCoroutineValueStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerUsingStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerTryStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

// Executes expression lowering with an optional bottom frame that owns the
// statement continuation. Ordinary expression callers pass no such frame.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionWithStatementFrame(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const ast::AstNode* statementContinuation,
    std::string& error)
{
    std::unique_ptr<ProjectOopHirNode> lowered;

    const std::optional<IterativeExpressionKind> rootKind =
        iterativeExpressionKind(expression, addressContext);
    if (rootKind.has_value() || statementContinuation != nullptr)
    {
        std::vector<IterativeExpressionFrame> frames;
        const std::size_t rootCleanupDepth = activeHirCleanupRegions_.size();
        const std::size_t rootExceptionHandlerDepth =
            activeHirExceptionHandlers_.size();
        if (statementContinuation != nullptr)
        {
            IterativeExpressionFrame frame;
            if (!initializeStatementContinuationFrame(
                    frame,
                    statementContinuation,
                    memberOrdinal,
                    addressContext,
                    error))
            {
                return nullptr;
            }
            frames.push_back(std::move(frame));
        }
        const ast::AstNode* current = expression;
        bool currentAddressContext = addressContext;
        std::size_t currentMemberOrdinal = memberOrdinal;
        IterativeInputKind currentInputKind = statementContinuation != nullptr &&
                statementContinuation->kind() == ast::AstNodeKind::TryStatement
            ? IterativeInputKind::LoopBlock
            : IterativeInputKind::Expression;
        bool worklistComplete = false;
        if (!initializeRootLoopWorklist(
                frames,
                current,
                currentMemberOrdinal,
                currentAddressContext,
                currentInputKind,
                error,
                worklistComplete,
                rootCleanupDepth,
                rootExceptionHandlerDepth) ||
            !initializeRootDeclarationWorklist(
                frames,
                current,
                currentAddressContext,
                lowered,
                error,
                worklistComplete,
                statementContinuation))
        {
            return nullptr;
        }
        while (!worklistComplete)
        {
            const bool schedulerResult = runExpressionScheduler(
                frames,
                current,
                currentMemberOrdinal,
                currentAddressContext,
                currentInputKind,
                lowered,
                error,
                worklistComplete,
                rootCleanupDepth,
                rootExceptionHandlerDepth);
            if (schedulerResult) break;
            if (resumePendingConditionStatementAfterFailure(
                    frames,
                    current,
                    currentMemberOrdinal,
                    currentAddressContext,
                    currentInputKind,
                    lowered,
                    error))
            {
                continue;
            }
            rollbackMatchCleanups(
                frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return nullptr;
        }
    }
    else
    {
        lowered = lowerExpressionImpl(expression, memberOrdinal, addressContext, error);
    }
    applyRootExpressionPostconditions(
        lowered, expression, memberOrdinal, statementContinuation, error);
    return lowered;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::buildConditionalExpression(
    const ast::AstNode* expression,
    std::unique_ptr<ProjectOopHirNode> condition,
    std::unique_ptr<ProjectOopHirNode> whenTrue,
    std::unique_ptr<ProjectOopHirNode> whenFalse)
{
    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";
    auto node = makeNode(ProjectOopHirNodeKind::Conditional, expression, resultType);
    convertConditionalBranch(whenTrue, expression->second(), resultType);
    convertConditionalBranch(whenFalse, expression->third(), resultType);
    node->children.push_back(std::move(condition));
    node->children.push_back(std::move(whenTrue));
    node->children.push_back(std::move(whenFalse));
    return node;
}

void ProjectOopHirModel::convertConditionalBranch(
    std::unique_ptr<ProjectOopHirNode>& branch,
    const ast::AstNode* syntax,
    const std::string& resultType)
{
    if (branch->typeKey == resultType) return;
    const MethodExpressionTypeBinding* binding = expressionType(syntax);
    const ProjectImplicitConversion conversion = ProjectTypeConversion::classifyImplicit(
        branch->typeKey,
        resultType,
        binding != nullptr ? binding->integerConstant : std::nullopt);
    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
        branch = wrapImplicitConversion(std::move(branch), conversion, syntax);
}

bool ProjectOopHirModel::isEnumTypeWithIdentity(
    std::string_view identity,
    const NamedTypeDeclaration& type) noexcept
{
    return type.kind == NamedTypeDeclarationKind::Enum &&
        type.identity.key() == identity;
}

bool ProjectOopHirModel::isEnumBinaryTypeKey(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view identity = typeKey.substr(5);
    return std::any_of(
        nameLookup_->namedTypes().begin(),
        nameLookup_->namedTypes().end(),
        std::bind_front(&ProjectOopHirModel::isEnumTypeWithIdentity, identity));
}

bool ProjectOopHirModel::isZeroIntegerLiteral(const ast::AstNode* syntax) noexcept
{
    return syntax != nullptr &&
        syntax->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
        syntax->integerValue() == 0;
}

void ProjectOopHirModel::convertBinaryEnumZeroOperands(
    const ast::AstNode* expression,
    std::unique_ptr<ProjectOopHirNode>& left,
    std::unique_ptr<ProjectOopHirNode>& right)
{
    if (expression->tokenKind() != lexer::TokenKind::EqualEqual &&
        expression->tokenKind() != lexer::TokenKind::BangEqual)
        return;
    if (isEnumBinaryTypeKey(left->typeKey) && isZeroIntegerLiteral(expression->second()))
    {
        auto converted = makeNode(
            ProjectOopHirNodeKind::Conversion, expression->second(), left->typeKey);
        converted->children.push_back(std::move(right));
        right = std::move(converted);
    }
    else if (isEnumBinaryTypeKey(right->typeKey) &&
             isZeroIntegerLiteral(expression->first()))
    {
        auto converted = makeNode(
            ProjectOopHirNodeKind::Conversion, expression->first(), right->typeKey);
        converted->children.push_back(std::move(left));
        left = std::move(converted);
    }
}

std::optional<std::string> ProjectOopHirModel::numericBinaryOperandType(
    const ast::AstNode* expression,
    const ProjectOopHirNode& left,
    const ProjectOopHirNode& right,
    const std::string& resultType) const
{
    if (ProjectTypeConversion::isNumeric(resultType)) return resultType;
    if (!isComparison(expression->tokenKind()) ||
        !ProjectTypeConversion::isNumeric(left.typeKey) ||
        !ProjectTypeConversion::isNumeric(right.typeKey))
        return std::nullopt;
    const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
    const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
    return ProjectTypeConversion::commonNumericType(
        left.typeKey,
        right.typeKey,
        leftType != nullptr ? leftType->integerConstant : std::nullopt,
        rightType != nullptr ? rightType->integerConstant : std::nullopt);
}

void ProjectOopHirModel::convertNumericBinaryOperands(
    const ast::AstNode* expression,
    const std::string& numericOperandType,
    std::unique_ptr<ProjectOopHirNode>& left,
    std::unique_ptr<ProjectOopHirNode>& right)
{
    const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
    const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
    const ProjectImplicitConversion leftConversion = ProjectTypeConversion::classifyImplicit(
        left->typeKey,
        numericOperandType,
        leftType != nullptr ? leftType->integerConstant : std::nullopt);
    const ProjectImplicitConversion rightConversion = ProjectTypeConversion::classifyImplicit(
        right->typeKey,
        numericOperandType,
        rightType != nullptr ? rightType->integerConstant : std::nullopt);
    if (leftConversion.exists() && leftConversion.kind != ProjectConversionKind::Identity)
        left = wrapImplicitConversion(std::move(left), leftConversion, expression->first());
    if (rightConversion.exists() && rightConversion.kind != ProjectConversionKind::Identity)
        right = wrapImplicitConversion(std::move(right), rightConversion, expression->second());
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::buildBuiltinBinaryExpression(
    const ast::AstNode* expression,
    std::unique_ptr<ProjectOopHirNode> left,
    std::unique_ptr<ProjectOopHirNode> right)
{
    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";
    auto node = makeNode(ProjectOopHirNodeKind::Binary, expression, resultType);
    node->operation = expression->tokenKind();
    node->booleanValue = typeBinding != nullptr && typeBinding->flowRuntimeCheckElided;
    convertBinaryEnumZeroOperands(expression, left, right);
    const std::optional<std::string> numericOperandType =
        numericBinaryOperandType(expression, *left, *right, resultType);
    if (numericOperandType.has_value())
        convertNumericBinaryOperands(expression, *numericOperandType, left, right);
    node->children.push_back(std::move(left));
    node->children.push_back(std::move(right));
    return node;
}

bool ProjectOopHirModel::hasValidDelegateCreationTarget(
    const MethodDelegateBinding& binding) const noexcept
{
    if (!binding.selectedMemberOrdinal.has_value() || memberModel_ == nullptr)
        return false;
    if (*binding.selectedMemberOrdinal < memberModel_->members().size())
        return true;
    return namespaceFunctionModel_ != nullptr &&
        *binding.selectedMemberOrdinal - memberModel_->members().size() <
            namespaceFunctionModel_->functions().size();
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishDelegateCreation(
    const ast::AstNode* expression,
    const MethodDelegateBinding& binding,
    std::unique_ptr<ProjectOopHirNode> receiver)
{
    auto node = makeNode(
        ProjectOopHirNodeKind::DelegateCreate,
        expression,
        binding.delegateTypeKey);
    node->elementTypeKey = binding.delegateTypeKey;
    node->memberOrdinal = binding.selectedMemberOrdinal;
    node->booleanValue = binding.virtualDispatch;
    node->interfaceDispatch = binding.interfaceDispatch;
    node->ownership = binding.ownsReceiver
        ? sema::OwnershipKind::Owned : sema::OwnershipKind::Borrowed;
    if (binding.interfaceDispatch)
        node->targetTypeOrdinal = binding.receiverTypeOrdinal;
    if (!binding.staticTarget)
        node->children.push_back(std::move(receiver));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerThisExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const std::string& resultType,
    std::string& error)
{
    if (currentLambda_ != nullptr)
    {
        const std::optional<std::size_t> capture = currentLambdaThisCapture();
        if (!capture.has_value())
        {
            error = "Stage 43 lambda this-reference has no captured receiver";
            return nullptr;
        }
        auto node = makeNode(
            addressContext && isStructValueType(resultType)
                ? ProjectOopHirNodeKind::ClosureCaptureAddress
                : ProjectOopHirNodeKind::ClosureCaptureValue,
            expression,
            resultType);
        node->integerValue = *capture;
        return node;
    }
    // `this` inside a STRUCT method is the receiver's ADDRESS -- struct receivers are
    // passed by address and memory-homed since Stage 40.2 -- so in a VALUE context the
    // expression `this` has to load through it, exactly the way a `ref` parameter does a
    // few cases below. Handing back the bare ThisValue there was two reported bugs at
    // once, and they are the same line:
    //
    //   * `Get(this)` passed the POINTER where the callee's signature wanted the
    //     aggregate; the LLVM verifier rejected the module. Loud.
    //   * `S8 self = this;` stored the pointer BITS into the local's own bytes -- for an
    //     8-byte struct the carrier is `i64`, so `store ptr` fit and nothing complained --
    //     and every later field read decoded those bits as the struct's fields. Silent,
    //     no diagnostic, passes the verifier, and it is the one that reaches production.
    //     It was also, bitterly, the workaround people wrote for the loud one.
    //
    // Nothing changes for a CLASS: `this` IS the reference, and its value in both
    // contexts is that pointer. Nothing changes in an ADDRESS context either -- and every
    // existing consumer of ThisValue is one: `lowerReceiver` lowers a struct receiver
    // with addressContext = true (so `this.field` and `this.Method()` keep taking the
    // address), and the constructor-initializer and instance-initializer helpers build
    // ThisValue directly as a receiver.
    auto self = makeNode(ProjectOopHirNodeKind::ThisValue, expression, resultType);
    if (addressContext || !isStructValueType(resultType))
    {
        return self;
    }
    auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
    load->children.push_back(std::move(self));
    return load;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNullConditionalMemberExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodExpressionTypeBinding* typeBinding,
    const std::string& resultType,
    std::string& error)
{
    if (addressContext)
    {
        error = "a null-conditional access produces a value, not addressable storage";
        return nullptr;
    }
    const MethodExpressionTypeBinding* receiverFlow = expressionType(expression->first());
    const bool provenNonNull = receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
    if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
        accessorCall != nullptr)
    {
        auto call = lowerResolvedCall(
            expression,
            memberOrdinal,
            addressContext,
            error,
            typeBinding,
            accessorCall,
            {},
            nullptr,
            !provenNonNull);
        if (call == nullptr) return nullptr;
        if (provenNonNull) call->stringValue = "flow-nonnull";
        return call;
    }
    const MethodNameBinding* binding = nameBinding(expression);
    if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
        binding->kind != MethodNameBindingKind::ExplicitObjectField)
    {
        error = "T01-F03 null-conditional access has no unique instance-field binding";
        return nullptr;
    }
    auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (receiver == nullptr) return nullptr;
    auto node = makeNode(
        provenNonNull ? ProjectOopHirNodeKind::FieldValue
                      : ProjectOopHirNodeKind::NullConditionalFieldValue,
        expression, resultType);
    node->memberOrdinal = binding->candidateMemberOrdinals.front();
    node->booleanValue = provenNonNull;
    node->volatileAccess = memberModel_ != nullptr &&
        *node->memberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[*node->memberOrdinal].isVolatile;
    node->children.push_back(std::move(receiver));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNameExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    const MethodCoreBinding* core = coreBinding(expression);
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::FixedArrayLength ||
         core->kind == MethodCoreOperationKind::ArrayLength ||
         core->kind == MethodCoreOperationKind::StringLength ||
         core->kind == MethodCoreOperationKind::IteratorCurrent))
    {
        return lowerCoreNameExpression(
            expression, memberOrdinal, addressContext, *core, error);
    }
    // NSC-LIMIT-008: la misma pregunta que hace isNaturallyAddressable, hecha una sola
    // vez. Antes estaba escrita aqui a mano y alli no se hacia, y esa discrepancia era el
    // defecto: un getter se clasificaba como direccionable y lowerReceiver no le hacia
    // temporal.
    if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
        accessorCall != nullptr)
    {
        const std::size_t accessorOrdinal = *accessorCall->selectedMemberOrdinal;
        if (addressContext && !memberModel_->members()[accessorOrdinal].isReferenceReturn)
        {
            error = "Stage 38 properties are semantic accessor calls and are not addressable";
            return nullptr;
        }
        return lowerBoundCall(
            expression,
            memberOrdinal,
            addressContext,
            error,
            expressionType(expression),
            accessorCall,
            {});
    }
    // NS304-T03-F03: a marked list-pattern length member that resolved to a
    // zero-argument `Length()` method was auto-invoked in Sema; lower it as the
    // ordinary instance call it became.
    if (const MethodCallBinding* lengthCall = callBinding(expression);
        lengthCall != nullptr && expression->auxiliaryText() == "list-pattern-length")
    {
        return lowerBoundCall(
            expression,
            memberOrdinal,
            addressContext,
            error,
            expressionType(expression),
            lengthCall,
            {});
    }
    // NS304-T08-F02: extension property getter auto-invocado en Sema (namespace call con
    // receptor reenviado). Se baja igual que la llamada de namespace del path de Call.
    if (const NamespaceFunctionCallBinding* namespaceCall =
            namespaceFunctionCallBinding(expression);
        namespaceCall != nullptr)
    {
        return lowerExtensionPropertyGetterWithReceiver(
            expression, memberOrdinal, error, *namespaceCall);
    }
    if (currentLambda_ != nullptr)
    {
        const MethodLambdaValueBinding* lambdaValue = lambdaValueBinding(expression);
        if (lambdaValue != nullptr &&
            lambdaValue->lambdaOrdinal == currentLambda_->lambdaOrdinal)
        {
            return lowerLambdaNameExpression(expression, addressContext);
        }
    }
    const MethodNameBinding* binding = nameBinding(expression);
    if (binding == nullptr)
    {
        error = "Stage 34 cannot map identifier/member expression to Stage 27 binding";
        return nullptr;
    }
    return lowerBoundNameExpression(
        expression, memberOrdinal, addressContext, *binding, error);
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerInterpolatedStringExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (const MethodCoreBinding* core = coreBinding(expression);
        core != nullptr && core->kind == MethodCoreOperationKind::InterpolationHandler)
    {
        if (addressContext)
        {
            error = "an interpolation handler result is a value, not addressable storage";
            return nullptr;
        }
        const std::size_t tempOrdinal = nextSyntheticLocalOrdinal_++;
        std::vector<language::InterpolationPart> parts;
        std::string parseError;
        if (!language::parseStaticInterpolation(expression->text(), parts, parseError))
        {
            error = std::move(parseError);
            return nullptr;
        }
        const bool hasHole = std::any_of(
            parts.begin(), parts.end(), &ProjectOopHirModel::isInterpolationHole);
        if (hasHole)
        {
            error = "interpolation handler hole has no expression";
            return nullptr;
        }
        return buildInterpolationHandlerExpression(
            expression, *core, tempOrdinal, parts, {}, error);
    }
    if (addressContext)
    {
        error = "a static interpolated string is immutable and not addressable";
        return nullptr;
    }
    const std::string* value = methodBodyModel_ != nullptr
        ? methodBodyModel_->staticInterpolationValue(expression) : nullptr;
    if (value == nullptr)
    {
        error = "T01-F06 static interpolation has no compile-time handler result";
        return nullptr;
    }
    auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
    node->stringValue = *value;
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerUnaryExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const std::string& resultType,
    std::string& error)
{
    if (expression->tokenKind() == lexer::TokenKind::PlusPlus ||
        expression->tokenKind() == lexer::TokenKind::MinusMinus)
    {
        // ID 79. `++x` es `x += 1` y `x++` es lo mismo devolviendo el valor anterior, asi
        // que los dos salen por el MISMO nodo que la asignacion compuesta y no por una
        // ruta paralela. La tarea 62 pedia extender esto "para punteros" cuando no existia
        // para ningun tipo; aqui entra para los dos a la vez, que es la unica forma de que
        // `++p` y `++i` no se separen.
        if (addressContext)
        {
            error = "the result of ++/-- is a value, not addressable storage";
            return nullptr;
        }
        const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
        if (targetType == nullptr)
        {
            error = "Stage 34 ++/-- operand has no resolved semantic type";
            return nullptr;
        }
        auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
        if (address == nullptr) return nullptr;

        auto step = makeNode(ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
        step->operation = expression->tokenKind() == lexer::TokenKind::PlusPlus
            ? lexer::TokenKind::Plus
            : lexer::TokenKind::Minus;
        // La UNICA diferencia entre postfijo y prefijo.
        step->booleanValue = expression->kind() == ast::AstNodeKind::PostfixExpression;

        std::unique_ptr<ProjectOopHirNode> one;
        if (ProjectTypeConversion::isPointer(targetType->typeKey))
        {
            // `++p` avanza UN ELEMENTO, no un byte. El tipo apuntado es lo que da el paso,
            // y lo resuelve el MIR con el mismo servicio de tamano que todo lo demas.
            step->elementTypeKey = targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
            one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nint");
            one->integerValue = 1;
        }
        else if (ProjectTypeConversion::isNumeric(targetType->typeKey) &&
                 !ProjectTypeConversion::isIntegral(targetType->typeKey))
        {
            one = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, targetType->typeKey);
            one->floatingValue = 1.0;
        }
        else
        {
            // El uno se construye YA en el tipo del destino, no en `int` con una conversion
            // detras: asi la operacion entera ocurre en un solo tipo y no hay una segunda
            // regla de promocion que mantener de acuerdo con la de `Binary`.
            one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, targetType->typeKey);
            one->integerValue = 1;
        }
        step->children.push_back(std::move(address));
        step->children.push_back(std::move(one));
        return step;
    }
    // Interop Foundation: `&lvalue` and `*pointer` are now first-class in the OOP
    // pipeline. HISTORY: both used to be walled off ("keeps raw pointer
    // address/dereference in the procedural backend boundary"), which made every
    // out-parameter Win32 API unusable -- `GetMessage(&msg)` could not produce a MSG*
    // from a local at all. Since Stage 40.2 every local is memory-homed (an alloca,
    // with indirect structs carried as [N x i8]), so an address-of is LITERALLY the
    // operand lowered in address context -- no new machinery, just returning what the
    // assignment path always computed for its own targets.
    //
    // These are the language's RAW tier -- same third reference category the Bloque 1
    // docs pin for plain aliases: no lifetime tracking, dangles if it outlives its
    // target, by design. C APIs are expressed through exactly this shape.
    if (expression->tokenKind() == lexer::TokenKind::Amp)
    {
        if (addressContext)
        {
            error = "Stage 34 address-of result is not itself addressable";
            return nullptr;
        }
        // Bit fields are logical slices of an allocation unit, not independently
        // addressable objects.  Assignment lowering may request their allocation-unit
        // address internally, but source `&field` must never expose that address as a
        // pointer to the logical field type.
        if (const MethodNameBinding* target = nameBinding(expression->first());
            target != nullptr && !target->candidateMemberOrdinals.empty() &&
            memberModel_ != nullptr &&
            target->candidateMemberOrdinals.front() < memberModel_->members().size() &&
            memberModel_->members()[target->candidateMemberOrdinals.front()].isBitField)
        {
            error = "bit fields have no address";
            return nullptr;
        }
        auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
        if (address == nullptr) return nullptr;
        // WRAPPED, never mutated: overwriting the operand node's typeKey was measured
        // to miscompile -- a ParameterValue whose typeKey was rewritten to the pointee
        // struct loaded the [N x i8] aggregate where the pointer belonged ("GEP base
        // pointer is not a vector..."). The wrapper carries the expression's type (T*)
        // for consumers that derive ABI decisions from operand typeKeys (call-site
        // indirect-argument wrapping reads exactly that), while the operand keeps its
        // own meaning.
        auto node = makeNode(ProjectOopHirNodeKind::AddressValue, expression, resultType);
        node->children.push_back(std::move(address));
        return node;
    }
    if (expression->tokenKind() == lexer::TokenKind::Star)
    {
        if (addressContext)
        {
            // The ADDRESS of `*p` is p's own value: `(*m).field = x` GEPs off it, and
            // `*p = v` stores straight through it.
            auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (pointer == nullptr) return nullptr;
            // Returned AS-IS: the pointer's value IS the address, and its own typeKey
            // (T*) is already the honest description of an address-of-T.
            return pointer;
        }
        auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
        if (pointer == nullptr) return nullptr;
        auto node = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
        node->children.push_back(std::move(pointer));
        return node;
    }
    auto node = makeNode(ProjectOopHirNodeKind::Unary, expression, resultType);
    node->operation = expression->tokenKind();
    auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (operand == nullptr)
    {
        return nullptr;
    }
    node->children.push_back(std::move(operand));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNullCoalescingExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const std::string& resultType,
    std::string& error)
{
    if (addressContext)
    {
        error = "a null-coalescing expression produces a value, not addressable storage";
        return nullptr;
    }
    auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
    auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (left == nullptr || right == nullptr) return nullptr;
    const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
    const ProjectImplicitConversion conversion = classifyImplicit(
        right->typeKey,
        resultType,
        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
    if (!conversion.exists())
    {
        error = "T01-F03 null-coalescing RHS cannot convert from '" + right->typeKey +
            "' to '" + resultType + "'";
        return nullptr;
    }
    if (conversion.kind != ProjectConversionKind::Identity)
        right = wrapImplicitConversion(std::move(right), conversion, expression->second());
    const MethodOptionalCarrierBinding* optional = optionalCarrierBinding(expression);
    auto node = makeNode(
        optional != nullptr ? ProjectOopHirNodeKind::OptionalCoalescing
                            : ProjectOopHirNodeKind::NullCoalescing,
        expression, resultType);
    if (optional != nullptr)
    {
        node->elementTypeKey = optional->carrierTypeKey;
        node->memberOrdinal = optional->hasValueMemberOrdinal;
        node->secondaryMemberOrdinal = optional->uncheckedValueMemberOrdinal;
    }
    node->children.push_back(std::move(left));
    node->children.push_back(std::move(right));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerBinaryExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    const MethodCoreBinding* stringCore = coreBinding(expression);
    // NSC-LIMIT-004. `a == b` y `a != b` sobre dos structs. El modelo semantico registro
    // un `StructEquals` sobre el propio BinaryExpression, asi que aqui se construye el
    // MISMO nodo que produce `a.Equals(b)` -- no hay una segunda semantica de igualdad de
    // agregados, solo dos formas de escribirla. La clase del nodo de sintaxis es lo que
    // las distingue: alli es una CallExpression, aqui un BinaryExpression.
    //
    // Para `!=` se envuelve la igualdad en el `Unary`/`Bang` que ya existe, en vez de
    // dar al nodo de agregados un modo negado propio: negar un bool es una instruccion, y
    // un segundo modo seria un segundo sitio donde la regla puede discrepar.
    if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::StructEquals)
    {
        if (addressContext)
        {
            error = "C8 struct comparison result is not addressable";
            return nullptr;
        }
        auto node = makeNode(
            ProjectOopHirNodeKind::AggregateEquals, expression, stringCore->resultTypeKey);
        node->elementTypeKey = stringCore->elementTypeKey;
        auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
        if (left == nullptr) return nullptr;
        auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
        if (right == nullptr) return nullptr;
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        node->argumentCount = node->children.size();
        if (!appendAggregateLeaves(
                stringCore->elementTypeKey, 0, expression, node->children, error))
        {
            return nullptr;
        }
        if (expression->tokenKind() == lexer::TokenKind::BangEqual)
        {
            auto negated = makeNode(
                ProjectOopHirNodeKind::Unary, expression, stringCore->resultTypeKey);
            negated->operation = lexer::TokenKind::Bang;
            negated->children.push_back(std::move(node));
            return negated;
        }
        return node;
    }
    if (stringCore != nullptr &&
        (stringCore->kind == MethodCoreOperationKind::StringConcat ||
         stringCore->kind == MethodCoreOperationKind::StringEquals))
    {
        if (addressContext)
        {
            error = "Stage 51 string operation result is not addressable";
            return nullptr;
        }
        auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
        auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
        if (left == nullptr || right == nullptr) return nullptr;
        auto node = makeNode(
            stringCore->kind == MethodCoreOperationKind::StringConcat
                ? ProjectOopHirNodeKind::StringConcat
                : ProjectOopHirNodeKind::StringEquals,
            expression,
            stringCore->resultTypeKey);
        node->operation = expression->tokenKind();
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        return node;
    }

    // CC1 fase 4D. El binding ya decidio que esto es aritmetica de punteros y con que
    // tipo de elemento; aqui solo se construye el nodo con los dos operandos EN EL ORDEN
    // EN QUE SE ESCRIBIERON. Ese orden importa: `i + p` y `p + i` dan lo mismo, pero
    // `p - i` y `i - p` no, y el backend necesita saber cual de los dos hijos es el
    // puntero. Lo deduce de los tipos de los hijos, no de una bandera aparte.
    if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::PointerArithmetic)
    {
        if (addressContext)
        {
            error = "CC1 pointer arithmetic yields a value, not addressable storage";
            return nullptr;
        }
        auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
        if (left == nullptr) return nullptr;
        auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
        if (right == nullptr) return nullptr;
        auto node = makeNode(
            ProjectOopHirNodeKind::PointerArithmetic, expression, stringCore->resultTypeKey);
        node->operation = expression->tokenKind();
        node->elementTypeKey = stringCore->elementTypeKey;
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        return node;
    }

    auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
    auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (left == nullptr || right == nullptr) return nullptr;
    return buildBuiltinBinaryExpression(
        expression, std::move(left), std::move(right));
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerRangeEndpoint(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const ast::AstNode* syntax,
    const ProjectImplicitConversion& conversion,
    std::string& error)
{
    if (syntax == nullptr)
    {
        auto zero = makeNode(
            ProjectOopHirNodeKind::IntegerConstant,
            expression,
            "builtin:int");
        zero->integerValue = 0;
        return zero;
    }
    auto endpoint = lowerExpression(syntax, memberOrdinal, false, error);
    if (endpoint == nullptr) return nullptr;
    if (conversion.exists() &&
        conversion.kind != ProjectConversionKind::Identity)
    {
        endpoint = wrapImplicitConversion(
            std::move(endpoint), conversion, syntax);
    }
    return endpoint;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerRangeSliceIndexExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodRangeSliceBinding* range,
    std::string& error)
{
    if (addressContext)
    {
        error = "T01-F04 a range slice is a value and cannot be used as assignable storage";
        return nullptr;
    }
    if (memberModel_ == nullptr ||
        range->lengthMemberOrdinal >= memberModel_->members().size() ||
        range->sliceMemberOrdinal >= memberModel_->members().size())
    {
        error = "T01-F04 range protocol lost its selected Length/Slice members";
        return nullptr;
    }
    auto receiver = lowerReceiver(
        range->receiverSyntax, range->receiverTypeKey, memberOrdinal, error);
    if (receiver == nullptr) return nullptr;
    auto start = lowerRangeEndpoint(
        expression, memberOrdinal, range->startSyntax, range->startConversion, error);
    auto end = lowerRangeEndpoint(
        expression, memberOrdinal, range->endSyntax, range->endConversion, error);
    if (start == nullptr || end == nullptr) return nullptr;

    const ClassMemberSymbol& sliceMember =
        memberModel_->members()[range->sliceMemberOrdinal];
    auto node = makeNode(
        ProjectOopHirNodeKind::RangeSlice,
        expression,
        range->resultTypeKey);
    node->elementTypeKey = range->receiverTypeKey;
    node->memberOrdinal = range->sliceMemberOrdinal;
    node->secondaryMemberOrdinal = range->lengthMemberOrdinal;
    node->referenceResult = sliceMember.isReferenceReturn;
    node->integerValue =
        (range->hasStart ? 1U : 0U) |
        (range->startFromEnd ? 2U : 0U) |
        (range->hasEnd ? 4U : 0U) |
        (range->endFromEnd ? 8U : 0U) |
        (range->boundsStaticallyProven ? 16U : 0U);
    node->argumentCount = 2;
    node->argumentParameterOrdinals = {0U, 1U};
    node->children.push_back(std::move(receiver));
    node->children.push_back(std::move(start));
    node->children.push_back(std::move(end));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerArrayOrStringIndexExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodCoreBinding* core,
    std::string& error)
{
    if (core->kind == MethodCoreOperationKind::StringElement && addressContext)
    {
        error = "Stage 41 strings are immutable; string elements are not assignable";
        return nullptr;
    }
    auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (receiver == nullptr) return nullptr;
    std::vector<const ast::AstNode*> indexSyntaxes;
    if (!expression->children().empty())
        indexSyntaxes.assign(expression->children().begin(), expression->children().end());
    else indexSyntaxes.push_back(expression->second());
    ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::StringElementValue;
    if (core->kind == MethodCoreOperationKind::ArrayElement)
    {
        kind = addressContext
            ? ProjectOopHirNodeKind::ArrayElementAddress
            : ProjectOopHirNodeKind::ArrayElementValue;
    }
    else if (core->kind == MethodCoreOperationKind::RangeArrayElement)
    {
        kind = addressContext
            ? ProjectOopHirNodeKind::RangeArrayElementAddress
            : ProjectOopHirNodeKind::RangeArrayElementValue;
    }
    auto node = makeNode(kind, expression, core->resultTypeKey);
    node->elementTypeKey = core->elementTypeKey;
    node->nativeArrayRank = core->kind == MethodCoreOperationKind::RangeArrayElement
        ? indexSyntaxes.size() : 1U;
    node->children.push_back(std::move(receiver));
    for (std::size_t dimension = 0; dimension < indexSyntaxes.size(); ++dimension)
    {
        const ast::AstNode* syntax = indexSyntaxes[dimension];
        const bool fromEnd = syntax != nullptr &&
            syntax->kind() == ast::AstNodeKind::FromEndExpression;
        if (dimension == 0U) node->booleanValue = fromEnd;
        if (fromEnd && dimension < 64U)
            node->integerValue |= (std::uint64_t{1} << dimension);
        auto index = lowerExpression(
            fromEnd ? syntax->first() : syntax, memberOrdinal, false, error);
        if (index == nullptr) return nullptr;
        node->children.push_back(std::move(index));
    }
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNullConditionalArrayIndexExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodCoreBinding* core,
    std::string& error)
{
    if (addressContext)
    {
        error = "T10-F05 null-conditional array element is not addressable";
        return nullptr;
    }
    auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (receiver == nullptr) return nullptr;
    const ast::AstNode* indexSyntax = !expression->children().empty()
        ? expression->children().front() : expression->second();
    auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
    if (index == nullptr) return nullptr;
    auto node = makeNode(
        ProjectOopHirNodeKind::NullConditionalArrayLoad, expression, core->resultTypeKey);
    node->elementTypeKey = core->elementTypeKey;
    node->children.push_back(std::move(receiver));
    node->children.push_back(std::move(index));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNullConditionalUserIndexExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodExpressionTypeBinding* typeBinding,
    std::string& error)
{
    if (addressContext)
    {
        error = "T10-F05 null-conditional user index is not addressable";
        return nullptr;
    }
    const MethodCallBinding* indexerCall = callBinding(expression);
    if (indexerCall == nullptr ||
        indexerCall->resolution != MethodCallResolutionKind::Resolved ||
        !indexerCall->selectedMemberOrdinal.has_value())
    {
        error = "T10-F05 null-conditional user index has no resolved get_Item accessor";
        return nullptr;
    }
    std::vector<const ast::AstNode*> arguments;
    if (!expression->children().empty())
        arguments.assign(expression->children().begin(), expression->children().end());
    else if (expression->second() != nullptr)
        arguments.push_back(expression->second());
    return lowerResolvedCall(
        expression,
        memberOrdinal,
        addressContext,
        error,
        typeBinding,
        indexerCall,
        arguments,
        nullptr,
        true);
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerVariableArrayIndexExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodCoreBinding* core,
    const MethodVariableArrayIndexBinding* variableArrayIndex,
    std::unique_ptr<ProjectOopHirNode> base,
    const std::vector<const ast::AstNode*>& indices,
    std::string& error)
{
    if (indices.size() != variableArrayIndex->extentLocalOrdinals.size())
    {
        error = "multidimensional VLA index lost its semantic rank";
        return nullptr;
    }
    std::unique_ptr<ProjectOopHirNode> linearIndex;
    for (std::size_t dimension = 0; dimension < indices.size(); ++dimension)
    {
        auto index = lowerExpression(indices[dimension], memberOrdinal, false, error);
        if (index == nullptr) return nullptr;
        if (index->typeKey != "builtin:nuint")
        {
            auto converted = makeNode(
                ProjectOopHirNodeKind::Conversion, indices[dimension], "builtin:nuint");
            converted->children.push_back(std::move(index));
            index = std::move(converted);
        }
        if (linearIndex == nullptr)
        {
            linearIndex = std::move(index);
            continue;
        }

        auto extent = makeNode(
            ProjectOopHirNodeKind::LocalValue,
            expression,
            methodBodyModel_->locals()[
                variableArrayIndex->extentLocalOrdinals[dimension]].typeKey);
        extent->localOrdinal =
            variableArrayIndex->extentLocalOrdinals[dimension];
        if (extent->typeKey != "builtin:nuint")
        {
            auto convertedExtent = makeNode(
                ProjectOopHirNodeKind::Conversion, expression, "builtin:nuint");
            convertedExtent->children.push_back(std::move(extent));
            extent = std::move(convertedExtent);
        }
        auto rowOffset = makeNode(
            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
        rowOffset->operation = lexer::TokenKind::Star;
        rowOffset->children.push_back(std::move(linearIndex));
        rowOffset->children.push_back(std::move(extent));
        auto sum = makeNode(
            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
        sum->operation = lexer::TokenKind::Plus;
        sum->children.push_back(std::move(rowOffset));
        sum->children.push_back(std::move(index));
        linearIndex = std::move(sum);
    }

    auto address = makeNode(
        ProjectOopHirNodeKind::RawElementAddress,
        expression,
        core->elementTypeKey);
    address->elementTypeKey = core->elementTypeKey;
    address->children.push_back(std::move(base));
    address->children.push_back(std::move(linearIndex));
    if (addressContext) return address;
    auto value = makeNode(
        ProjectOopHirNodeKind::PointerLoad,
        expression,
        core->resultTypeKey);
    value->children.push_back(std::move(address));
    return value;
}


void ProjectOopHirModel::configureFlexibleArrayBounds(
    ProjectOopHirNode* address,
    const ast::AstNode* baseSyntax,
    bool baseIsFlexibleArray)
{
    // NS304-T13-F06: a flexible-array index bounds-checks against its `Length`
    // member named by [TrailingStorage(nameof(Length), ...)]. Resolve its byte
    // offset here so the raw element address can emit the runtime guard.
    if (baseIsFlexibleArray && metaprogrammingModel_ != nullptr &&
        baseSyntax != nullptr && baseSyntax->first() != nullptr &&
        inheritanceModel_ != nullptr && memberModel_ != nullptr &&
        objectLayoutModel_ != nullptr)
    {
        const MethodExpressionTypeBinding* receiverType =
            expressionType(baseSyntax->first());
        if (receiverType != nullptr)
        {
            const std::optional<std::size_t> structOrdinal =
                inheritanceModel_->typeOrdinalForTypeKey(receiverType->typeKey);
            if (structOrdinal.has_value())
            {
                for (const StaticAttributeInfo& attribute :
                     metaprogrammingModel_->attributes())
                {
                    if (attribute.identity != "TrailingStorage" ||
                        attribute.constantArguments.empty() ||
                        !attribute.typeOrdinal.has_value() ||
                        attribute.typeOrdinal != structOrdinal)
                    {
                        continue;
                    }
                    const std::span<const std::size_t> lengthMembers =
                        memberModel_->findMembers(
                            *structOrdinal, attribute.constantArguments[0]);
                    if (!lengthMembers.empty() &&
                        attribute.memberOrdinal.has_value())
                    {
                        const ObjectFieldLayout* lengthLayout =
                            objectLayoutModel_->fieldLayoutForMember(
                                lengthMembers.front());
                        const ObjectFieldLayout* dataLayout =
                            objectLayoutModel_->fieldLayoutForMember(
                                *attribute.memberOrdinal);
                        if (lengthLayout != nullptr && dataLayout != nullptr)
                        {
                            address->hasTrailingLengthBounds = true;
                            address->trailingLengthRelativeOffset =
                                static_cast<std::int64_t>(lengthLayout->offset) -
                                static_cast<std::int64_t>(dataLayout->offset);
                        }
                    }
                    break;
                }
            }
        }
    }
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerOrdinaryRawElementIndexExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodCoreBinding* core,
    const MethodExpressionTypeBinding* baseType,
    bool baseIsFixedArray,
    bool baseIsFlexibleArray,
    std::unique_ptr<ProjectOopHirNode> base,
    const std::vector<const ast::AstNode*>& indices,
    std::string& error)
{
    const ast::AstNode* baseSyntax = expression->first();
    std::string indexedType = baseType != nullptr ? baseType->typeKey : std::string();
    std::unique_ptr<ProjectOopHirNode> address;
    for (const ast::AstNode* indexSyntax : indices)
    {
        const std::optional<std::string> fixedElement = baseIsFixedArray
            ? ProjectMemberModel::fixedArrayElementTypeKey(indexedType)
            : std::optional<std::string>();
        const std::optional<std::string> flexibleElement = baseIsFlexibleArray
            ? ProjectMemberModel::flexibleArrayElementTypeKey(indexedType)
            : std::optional<std::string>();
        const std::string elementType = fixedElement.has_value()
            ? *fixedElement
            : flexibleElement.has_value() ? *flexibleElement : core->elementTypeKey;
        auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
        if (index == nullptr) return nullptr;
        address = makeNode(
            ProjectOopHirNodeKind::RawElementAddress, expression, elementType);
        address->elementTypeKey = elementType;
        configureFlexibleArrayBounds(address.get(), baseSyntax, baseIsFlexibleArray);
        address->children.push_back(std::move(base));
        address->children.push_back(std::move(index));
        base = std::move(address);
        indexedType = elementType;
    }
    if (base == nullptr)
    {
        error = "fixed-array/raw-pointer index expression has no index";
        return nullptr;
    }
    if (addressContext) return base;
    auto value = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, core->resultTypeKey);
    value->children.push_back(std::move(base));
    return value;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerRawElementIndexExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodCoreBinding* core,
    std::string& error)
{
    const ast::AstNode* baseSyntax = expression->first();
    const MethodExpressionTypeBinding* baseType = expressionType(baseSyntax);
    const bool baseIsFixedArray = baseType != nullptr &&
        ProjectMemberModel::fixedArrayElementTypeKey(baseType->typeKey).has_value();
    const bool baseIsFlexibleArray = baseType != nullptr &&
        ProjectMemberModel::flexibleArrayElementTypeKey(baseType->typeKey).has_value();
    auto base = lowerExpression(
        baseSyntax, memberOrdinal, baseIsFixedArray || baseIsFlexibleArray, error);
    if (base == nullptr) return nullptr;
    std::vector<const ast::AstNode*> indices;
    if (!expression->children().empty())
        indices.assign(expression->children().begin(), expression->children().end());
    else if (expression->second() != nullptr)
        indices.push_back(expression->second());

    const MethodVariableArrayIndexBinding* variableArrayIndex =
        methodBodyModel_ != nullptr
            ? methodBodyModel_->variableArrayIndexBinding(expression)
            : nullptr;
    if (variableArrayIndex != nullptr)
    {
        return lowerVariableArrayIndexExpression(
            expression,
            memberOrdinal,
            addressContext,
            core,
            variableArrayIndex,
            std::move(base),
            indices,
            error);
    }
    return lowerOrdinaryRawElementIndexExpression(
        expression,
        memberOrdinal,
        addressContext,
        core,
        baseType,
        baseIsFixedArray,
        baseIsFlexibleArray,
        std::move(base),
        indices,
        error);
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerIndexerAccessorExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodExpressionTypeBinding* typeBinding,
    const MethodCallBinding* indexerCall,
    std::string& error)
{
    const ClassMemberSymbol& getter =
        memberModel_->members()[*indexerCall->selectedMemberOrdinal];
    if (addressContext && !getter.isReferenceReturn)
    {
        error = "Stage 38 indexers are semantic accessor calls and are not directly addressable";
        return nullptr;
    }
    std::vector<const ast::AstNode*> arguments;
    if (!expression->children().empty())
    {
        arguments.insert(arguments.end(), expression->children().begin(), expression->children().end());
    }
    else if (expression->second() != nullptr)
    {
        arguments.push_back(expression->second());
    }
    return lowerResolvedCall(
        expression,
        memberOrdinal,
        addressContext,
        error,
        typeBinding,
        indexerCall,
        arguments);
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerIndexExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodExpressionTypeBinding* typeBinding,
    std::string& error)
{
    if (const MethodRangeSliceBinding* range = rangeSliceBinding(expression);
        range != nullptr)
    {
        return lowerRangeSliceIndexExpression(
            expression, memberOrdinal, addressContext, range, error);
    }
    const MethodCoreBinding* core = coreBinding(expression);
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::ArrayElement ||
         core->kind == MethodCoreOperationKind::RangeArrayElement ||
         core->kind == MethodCoreOperationKind::StringElement))
    {
        return lowerArrayOrStringIndexExpression(
            expression, memberOrdinal, addressContext, core, error);
    }
    // NS304-T10-F05: `receiver?[index]` — load condicional de elemento de array nativo.
    if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalArrayElement)
    {
        return lowerNullConditionalArrayIndexExpression(
            expression, memberOrdinal, addressContext, core, error);
    }
    // NS304-T10-F05: `receiver?[index]` sobre un indexador de usuario — null-check +
    // llamada al get_Item (receptor evaluado una vez por lowerResolvedCall).
    if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalUserIndex)
    {
        return lowerNullConditionalUserIndexExpression(
            expression, memberOrdinal, addressContext, typeBinding, error);
    }
    // C2.1 "fixed arrays": inline, unmanaged element access -- `fixedArrayField[i]` and
    // `rawPointer[i]`. The only difference between the two is how the BASE ADDRESS is
    // obtained, and it is a real difference: a fixed array IS its storage, so its address
    // comes from lowering the base in address context; a pointer HOLDS an address, so its
    // address comes from lowering the base as an ordinary value. Everything after that --
    // the scaled GEP, the load, the store -- is identical, which is why one node covers
    // both.
    if (core != nullptr && core->kind == MethodCoreOperationKind::RawElement)
    {
        return lowerRawElementIndexExpression(
            expression, memberOrdinal, addressContext, core, error);
    }

    const MethodCallBinding* indexerCall = callBinding(expression);
    if (indexerCall != nullptr && indexerCall->resolution == MethodCallResolutionKind::Resolved &&
        indexerCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *indexerCall->selectedMemberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[*indexerCall->selectedMemberOrdinal].isIndexerAccessor &&
        memberModel_->members()[*indexerCall->selectedMemberOrdinal].isGetter)
    {
        return lowerIndexerAccessorExpression(
            expression, memberOrdinal, addressContext, typeBinding, indexerCall, error);
    }
    error = "Stage 38 OOP lowering requires class/interface index expressions to resolve to an indexer accessor";
    return nullptr;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNullCoalescingAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (addressContext)
    {
        error = "the result of null-coalescing assignment is a value, not addressable storage";
        return nullptr;
    }
    const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
    if (targetType == nullptr)
    {
        error = "T01-F03 null-coalescing assignment target has no semantic type";
        return nullptr;
    }
    auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
    auto fallback = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (address == nullptr || fallback == nullptr) return nullptr;
    const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
    const ProjectImplicitConversion conversion = classifyImplicit(
        fallback->typeKey,
        targetType->typeKey,
        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
    if (!conversion.exists())
    {
        error = "T01-F03 null-coalescing assignment RHS cannot convert from '" +
            fallback->typeKey + "' to '" + targetType->typeKey + "'";
        return nullptr;
    }
    if (conversion.kind != ProjectConversionKind::Identity)
        fallback = wrapImplicitConversion(
            std::move(fallback), conversion, expression->second());
    auto node = makeNode(
        ProjectOopHirNodeKind::NullCoalescingAssignment,
        expression,
        targetType->typeKey);
    node->children.push_back(std::move(address));
    node->children.push_back(std::move(fallback));
    return node;
}


bool ProjectOopHirModel::expandAssignmentTargetTuple(
    std::string& error,
    const ast::AstNode* targetSyntax,
    const std::string& sourceTypeKey,
    std::uint64_t byteOffset,
    std::vector<HirTuplePatternFrame>& children)
{
    const std::optional<std::vector<std::string>> nestedElements =
        ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
    const std::optional<language::AggregateLayout> nestedLayout =
        objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, sourceTypeKey);
    if (!nestedElements.has_value() || !nestedLayout.has_value() ||
        nestedElements->size() != targetSyntax->children().size() ||
        nestedLayout->offsets.size() != nestedElements->size())
    {
        error = "T02-F03 nested assignment target lost its tuple layout";
        return false;
    }
    children.reserve(nestedElements->size());
    for (std::size_t index = 0; index < nestedElements->size(); ++index)
        children.push_back(HirTuplePatternFrame{
            targetSyntax->children()[index],
            (*nestedElements)[index],
            byteOffset + nestedLayout->offsets[index]});
    return true;
}


bool ProjectOopHirModel::appendAssignmentTargetLeaf(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    ProjectOopHirNode* node,
    std::string& error,
    const ast::AstNode* targetSyntax,
    const std::string& sourceTypeKey,
    std::uint64_t byteOffset)
{
    if (targetSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
        targetSyntax->text() == "_") return true;
    const MethodExpressionTypeBinding* targetType = expressionType(targetSyntax);
    if (targetType == nullptr)
    {
        error = "T02-F03 deconstruction target has no semantic type";
        return false;
    }
    auto address = lowerExpression(targetSyntax, memberOrdinal, true, error);
    if (address == nullptr) return false;
    const ProjectImplicitConversion conversion = classifyImplicit(
        sourceTypeKey, targetType->typeKey, std::nullopt);
    if (!conversion.exists())
    {
        error = "T02-F03 deconstruction lost its validated element conversion";
        return false;
    }
    auto element = makeNode(
        ProjectOopHirNodeKind::TupleDeconstructElement,
        targetSyntax,
        targetType->typeKey);
    element->elementTypeKey = sourceTypeKey;
    element->integerValue = byteOffset;
    element->conversion = conversion;
    element->children.push_back(std::move(address));
    node->children.push_back(std::move(element));
    return true;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerTupleDeconstructionAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (addressContext)
    {
        error = "tuple deconstruction result is a value, not addressable storage";
        return nullptr;
    }
    const MethodDeconstructionBinding* binding =
        methodBodyModel_->deconstructionBinding(expression);
    if (binding == nullptr)
    {
        error = "T02-F03 deconstruction assignment has no semantic binding";
        return nullptr;
    }
    auto source = lowerDeconstructionSource(
        expression, expression->second(), memberOrdinal, error);
    if (source == nullptr) return nullptr;
    const std::optional<std::vector<std::string>> sourceElements =
        ProjectTypeConversion::tupleElementTypeKeys(binding->tupleTypeKey);
    if (!sourceElements.has_value() || objectLayoutModel_ == nullptr ||
        nameLookup_ == nullptr)
    {
        error = "T02-F03 deconstruction source has no tuple layout";
        return nullptr;
    }
    auto node = makeNode(
        ProjectOopHirNodeKind::TupleDeconstruct, expression, binding->tupleTypeKey);
    node->children.push_back(std::move(source));
    auto expandTargetTuple = std::bind_front(
        &ProjectOopHirModel::expandAssignmentTargetTuple, this, std::ref(error));
    auto appendTargetLeaf = std::bind_front(
        &ProjectOopHirModel::appendAssignmentTargetLeaf,
        this,
        expression,
        memberOrdinal,
        node.get(),
        std::ref(error));
    if (!visitHirTuplePatternLeaves(
            expression->first(),
            binding->tupleTypeKey,
            0,
            expandTargetTuple,
            appendTargetLeaf))
    {
        return nullptr;
    }
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEventAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodEventBinding* stage42Event,
    std::string& error)
{
    if (addressContext)
    {
        error = "Stage 42 event subscription expression is not addressable";
        return nullptr;
    }
    if (stage42Event->customAccessorMemberOrdinal.has_value())
    {
        // NS304-T09-F08: custom accessors -> instancia una llamada a add_/remove_.
        auto node = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, "builtin:void");
        node->memberOrdinal = *stage42Event->customAccessorMemberOrdinal;
        if (!stage42Event->staticEvent)
        {
            auto receiver = lowerReceiver(
                stage42Event->receiverSyntax, stage42Event->receiverTypeKey, memberOrdinal, error);
            if (receiver == nullptr) return nullptr;
            node->children.push_back(std::move(receiver));
        }
        auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
        if (handler == nullptr) return nullptr;
        if (!stage42Event->argumentConversions.empty() &&
            stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
        {
            handler = wrapImplicitConversion(
                std::move(handler), stage42Event->argumentConversions.front(), expression->second());
        }
        node->children.push_back(std::move(handler));
        node->argumentParameterOrdinals.push_back(0);
        node->argumentTypeKeys.push_back(stage42Event->delegateTypeKey);
        return node;
    }
    auto backingAddress = lowerEventBacking(
        expression, memberOrdinal, error, stage42Event, true);
    auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (backingAddress == nullptr || handler == nullptr) return nullptr;
    if (!stage42Event->argumentConversions.empty() &&
        stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
    {
        handler = wrapImplicitConversion(
            std::move(handler), stage42Event->argumentConversions.front(), expression->second());
    }
    auto node = makeNode(
        stage42Event->kind == MethodEventOperationKind::Add
            ? ProjectOopHirNodeKind::EventAdd : ProjectOopHirNodeKind::EventRemove,
        expression, "builtin:void");
    node->elementTypeKey = stage42Event->delegateTypeKey;
    node->memberOrdinal = stage42Event->backingMemberOrdinal;
    node->children.push_back(std::move(backingAddress));
    node->children.push_back(std::move(handler));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerCompoundSetterAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodExpressionTypeBinding* typeBinding,
    const MethodCallBinding* setterCall,
    const std::vector<const ast::AstNode*>& arguments,
    std::string& error)
{
    if (addressContext)
    {
        error = "the result of a compound assignment is a value, not addressable storage";
        return nullptr;
    }
    const lexer::TokenKind baseOperator =
        lexer::compoundAssignmentBaseOperator(expression->tokenKind());
    if (baseOperator == lexer::TokenKind::Invalid)
    {
        error = "unsupported compound assignment operator on a property/indexer";
        return nullptr;
    }
    auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (current == nullptr) return nullptr;
    auto operand = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (operand == nullptr) return nullptr;
    const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
    const ProjectImplicitConversion conversion = classifyImplicit(
        operand->typeKey,
        current->typeKey,
        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
    const bool shift = baseOperator == lexer::TokenKind::LessLess ||
        baseOperator == lexer::TokenKind::GreaterGreater;
    if (!shift && !conversion.exists())
    {
        error = "compound assignment to a property/indexer cannot convert its "
            "right-hand side from '" + operand->typeKey + "' to '" +
            current->typeKey + "'";
        return nullptr;
    }
    if (!shift && conversion.kind != ProjectConversionKind::Identity)
    {
        operand = wrapImplicitConversion(
            std::move(operand), conversion, expression->second());
    }
    auto combined = makeNode(
        ProjectOopHirNodeKind::Binary, expression, current->typeKey);
    combined->operation = baseOperator;
    combined->children.push_back(std::move(current));
    combined->children.push_back(std::move(operand));
    auto setterNode = lowerResolvedCall(
        expression,
        memberOrdinal,
        addressContext,
        error,
        typeBinding,
        setterCall,
        arguments,
        std::move(combined));
    if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
    return setterNode;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerSetterAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodExpressionTypeBinding* typeBinding,
    const MethodCallBinding* setterCall,
    const ClassMemberSymbol& setter,
    std::string& error)
{
    std::vector<const ast::AstNode*> arguments;
    if (setter.isIndexerAccessor && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IndexExpression)
    {
        const ast::AstNode* indexExpression = expression->first();
        if (!indexExpression->children().empty())
        {
            arguments.insert(
                arguments.end(), indexExpression->children().begin(), indexExpression->children().end());
        }
        else if (indexExpression->second() != nullptr)
        {
            arguments.push_back(indexExpression->second());
        }
    }
    arguments.push_back(expression->second());

    // ID 82. `p.X op= e` es `set_X(p, get_X(p) op e)`. El valor que recibe el
    // setter se arma AQUI, no se baja de la fuente.
    //
    // El operando izquierdo de esa operacion es la llamada al GETTER, y se obtiene
    // bajando `expression->first()` en contexto de VALOR: los casos
    // IdentifierExpression/MemberExpression e IndexExpression de esta misma
    // funcion ya saben convertir ese nodo en la llamada al getter, con su receptor
    // y sus argumentos de indice. No hay una segunda ruta de resolucion de
    // accesores aqui; se reusa la unica que hay.
    //
    // EL RECEPTOR Y LOS INDICES SE BAJAN DOS VECES -- una dentro de la llamada al
    // getter y otra para la del setter -- porque el arbol es de `unique_ptr` y un
    // nodo no se comparte entre dos padres. Eso NO es un descuido: el modelo
    // semantico ya rechazo toda forma en la que la segunda bajada pudiera dar algo
    // distinto (isAccessorDestinationReevaluable), asi que lo que llega hasta aqui
    // son locals, parametros, `this` y constantes. La diferencia con un destino
    // CON direccion es que aquel si tiene una direccion que bajar una sola vez, y
    // por eso ese caso lo resuelve `CompoundStore` unas lineas mas abajo.
    //
    // El operando derecho se baja UNA sola vez, dentro del `Binary`, DESPUES de la
    // llamada al getter -- el mismo orden que C#: receptor, indices, getter, lado
    // derecho, operacion, setter.
    //
    // La operacion ocurre entera en el tipo del destino, y la conversion del lado
    // derecho se clasifica con la MISMA llamada que usa `CompoundStore`. El modelo
    // semantico ya exigio que exista, asi que si no existe aqui es una discrepancia
    // entre capas y se reporta como tal, no se ignora.
    if (expression->tokenKind() != lexer::TokenKind::Equal)
    {
        return lowerCompoundSetterAssignmentExpression(
            expression,
            memberOrdinal,
            addressContext,
            typeBinding,
            setterCall,
            arguments,
            error);
    }
    auto setterNode = lowerResolvedCall(
        expression,
        memberOrdinal,
        addressContext,
        error,
        typeBinding,
        setterCall,
        arguments);
    if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
    return setterNode;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNamespaceSetterAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const NamespaceFunctionCallBinding* namespaceSetter,
    std::string& error)
{
    if (addressContext)
    {
        error = "the result of an extension-property assignment is a value, not addressable storage";
        return nullptr;
    }
    const std::size_t functionOrdinal = *namespaceSetter->selectedFunctionOrdinal;
    const NamespaceFunctionSymbol& target =
        namespaceFunctionModel_->functions()[functionOrdinal];
    auto node = makeNode(
        ProjectOopHirNodeKind::StaticCall, expression, namespaceSetter->resultTypeKey);
    node->namespaceFunctionOrdinal = functionOrdinal;
    node->stringValue = target.mangledName;
    if (namespaceSetter->forwardedReceiver)
    {
        if (namespaceSetter->receiverSyntax == nullptr || target.parameters.empty())
        {
            error = "extension setter lost its synthetic receiver contract";
            return nullptr;
        }
        auto receiver = lowerExpression(
            namespaceSetter->receiverSyntax, memberOrdinal, false, error);
        if (receiver == nullptr) return nullptr;
        if (!namespaceSetter->selectedConversions.empty() &&
            namespaceSetter->selectedConversions.front().kind !=
                ProjectConversionKind::Identity)
        {
            receiver = wrapImplicitConversion(
                std::move(receiver), namespaceSetter->selectedConversions.front(),
                namespaceSetter->receiverSyntax);
        }
        node->children.push_back(std::move(receiver));
        node->argumentParameterOrdinals.push_back(0);
        node->argumentTypeKeys.push_back(target.parameters[0].typeKey);
    }
    auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (value == nullptr) return nullptr;
    const std::size_t valueParameterOrdinal = namespaceSetter->forwardedReceiver ? 1 : 0;
    if (valueParameterOrdinal >= target.parameters.size())
    {
        error = "extension setter has no value parameter";
        return nullptr;
    }
    if (valueParameterOrdinal < namespaceSetter->selectedConversions.size() &&
        namespaceSetter->selectedConversions[valueParameterOrdinal].kind !=
            ProjectConversionKind::Identity)
    {
        value = wrapImplicitConversion(
            std::move(value),
            namespaceSetter->selectedConversions[valueParameterOrdinal],
            expression->second());
    }
    node->children.push_back(std::move(value));
    node->argumentParameterOrdinals.push_back(valueParameterOrdinal);
    node->argumentTypeKeys.push_back(target.parameters[valueParameterOrdinal].typeKey);
    node->returnsAssignedValue = true;
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNullConditionalMemberAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const std::string& resultType,
    std::string& error)
{
    const MethodNameBinding* binding = nameBinding(expression->first());
    if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
        binding->kind != MethodNameBindingKind::ExplicitObjectField)
    {
        error = "T10-F05 null-conditional assignment has no unique instance-field binding";
        return nullptr;
    }
    auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
    if (receiver == nullptr) return nullptr;
    auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (value == nullptr) return nullptr;
    auto node = makeNode(
        ProjectOopHirNodeKind::NullConditionalFieldStore, expression, resultType);
    node->operation = expression->tokenKind();
    const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
    node->booleanValue = assignmentType != nullptr &&
        assignmentType->ownsStorageDestination;
    node->memberOrdinal = binding->candidateMemberOrdinals.front();
    node->volatileAccess = memberModel_ != nullptr &&
        *node->memberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[*node->memberOrdinal].isVolatile;
    node->children.push_back(std::move(receiver));
    node->children.push_back(std::move(value));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNullConditionalIndexAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const std::string& resultType,
    std::string& error)
{
    const MethodCoreBinding* core = coreBinding(expression->first());
    if (core == nullptr ||
        core->kind != MethodCoreOperationKind::NullConditionalArrayElement)
    {
        error = "T10-F05 null-conditional index store has no native-array element binding";
        return nullptr;
    }
    auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
    if (receiver == nullptr) return nullptr;
    const ast::AstNode* indexSyntax = !expression->first()->children().empty()
        ? expression->first()->children().front() : expression->first()->second();
    auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
    if (index == nullptr) return nullptr;
    auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (value == nullptr) return nullptr;
    auto node = makeNode(
        ProjectOopHirNodeKind::NullConditionalArrayStore, expression, resultType);
    node->elementTypeKey = core->elementTypeKey;
    const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
    node->booleanValue = assignmentType != nullptr &&
        assignmentType->ownsStorageDestination;
    node->children.push_back(std::move(receiver));
    node->children.push_back(std::move(index));
    node->children.push_back(std::move(value));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::createOrdinaryAssignmentNode(
    const ast::AstNode* expression,
    const std::string& resultType)
{
    auto node = makeNode(ProjectOopHirNodeKind::Assignment, expression, resultType);
    node->operation = expression->tokenKind();
    if (node->operation == lexer::TokenKind::Equal &&
        expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const MethodNameBinding* targetBinding = nameBinding(expression->first());
        if (targetBinding != nullptr && targetBinding->localOrdinal.has_value() &&
            *targetBinding->localOrdinal < methodBodyModel_->locals().size() &&
            (methodBodyModel_->locals()[*targetBinding->localOrdinal].isRaii ||
             methodBodyModel_->locals()[*targetBinding->localOrdinal].requiresValueDrop))
        {
            node->booleanValue = true;
            node->localOrdinal = *targetBinding->localOrdinal;
        }
    }
    // Owning replacement is a semantic property of the destination, not an AST pattern.
    // Fields and elements of owned arrays both lower to an address-based store; the
    // semantic binding records which stores own their current value so this layer can
    // request the existing release-before-store path without rediscovering declarations.
    if (!node->booleanValue)
    {
        const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
        node->booleanValue = assignmentType != nullptr &&
            assignmentType->ownsStorageDestination;
    }
    // T03-F01/T06-F08: a move-only aggregate owns its current active payload even when
    // the replacement is a freshly returned value rather than an explicit move(local).
    // Mark every ordinary store of a drop-bearing value so the backend destroys the
    // displaced value after evaluating the RHS and before writing the replacement.
    if (!node->booleanValue && node->operation == lexer::TokenKind::Equal &&
        memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(resultType))
    {
        node->booleanValue = true;
    }
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerCompoundAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const std::string& resultType,
    std::unique_ptr<ProjectOopHirNode> node,
    std::string& error)
{
    // ID 81. Los diez operadores compuestos. El destino se baja UNA VEZ en contexto de
    // direccion -- si se desazucarara a `x = x + e` se evaluaria dos veces y
    // `a[f()] += 1` llamaria a `f` dos veces.
    if (addressContext)
    {
        error = "the result of a compound assignment is a value, not addressable storage";
        return nullptr;
    }
    // T41: `raii string += value` is an ownership-aware replacement, not a numeric
    // CompoundStore. The RAII destination is bound specially by semantic analysis,
    // so its identifier does not need a separate expression-type record here; the
    // assignment result type is the authoritative resolved type.
    if (node->operation == lexer::TokenKind::PlusEqual &&
        resultType == "builtin:string")
    {
        const MethodNameBinding* targetBinding = nameBinding(expression->first());
        if (targetBinding == nullptr || !targetBinding->localOrdinal.has_value())
        {
            error = "string += requires an identifier-backed RAII string destination";
            return nullptr;
        }
        auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
        auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
        auto suffix = lowerExpression(expression->second(), memberOrdinal, false, error);
        if (address == nullptr || current == nullptr || suffix == nullptr) return nullptr;

        auto concatenated = makeNode(
            ProjectOopHirNodeKind::StringConcat, expression, "builtin:string");
        concatenated->operation = lexer::TokenKind::Plus;
        concatenated->children.push_back(std::move(current));
        concatenated->children.push_back(std::move(suffix));

        node->operation = lexer::TokenKind::Equal;
        node->booleanValue = true;
        node->localOrdinal = *targetBinding->localOrdinal;
        node->children.push_back(std::move(address));
        node->children.push_back(std::move(concatenated));
        return node;
    }
    const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
    if (targetType == nullptr)
    {
        error = "Stage 34 compound assignment target has no resolved semantic type";
        return nullptr;
    }
    auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
    if (address == nullptr) return nullptr;
    auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (value == nullptr) return nullptr;

    auto compound = makeNode(
        ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
    compound->operation = lexer::compoundAssignmentBaseOperator(node->operation);
    if (compound->operation == lexer::TokenKind::Invalid)
    {
        error = "unsupported compound assignment operator";
        return nullptr;
    }
    // El valor de `x += e` es el NUEVO, como en C#.
    compound->booleanValue = false;

    if (ProjectTypeConversion::isPointer(targetType->typeKey))
    {
        compound->elementTypeKey =
            targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
    }
    else
    {
        // La operacion ocurre ENTERA en el tipo del destino. El modelo semantico ya
        // exigio que el operando derecho convierta a ese tipo (ver analyzeAssignment),
        // asi que aqui no hay ni un estrechamiento silencioso ni una segunda tabla de
        // promociones distinta de la de `Binary`.
        const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
        const ProjectImplicitConversion conversion = classifyImplicit(
            value->typeKey,
            targetType->typeKey,
            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
        const bool shift = compound->operation == lexer::TokenKind::LessLess ||
            compound->operation == lexer::TokenKind::GreaterGreater;
        if (!shift && !conversion.exists())
        {
            error = "compound assignment cannot convert its right-hand side from '" +
                value->typeKey + "' to '" + targetType->typeKey + "'";
            return nullptr;
        }
        if (!shift && conversion.kind != ProjectConversionKind::Identity)
        {
            value = wrapImplicitConversion(
                std::move(value), conversion, expression->second());
        }
    }
    compound->children.push_back(std::move(address));
    compound->children.push_back(std::move(value));
    return compound;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerSimpleAssignmentExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::unique_ptr<ProjectOopHirNode> node,
    std::string& error)
{
    auto target = lowerExpression(expression->first(), memberOrdinal, true, error);
    auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (target == nullptr || value == nullptr)
    {
        return nullptr;
    }
    const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
    const ProjectImplicitConversion conversion = classifyImplicit(
        value->typeKey,
        target->typeKey,
        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
    {
        value = wrapImplicitConversion(std::move(value), conversion, expression->second());
    }
    node->children.push_back(std::move(target));
    node->children.push_back(std::move(value));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerAssignmentExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodExpressionTypeBinding* typeBinding,
    const std::string& resultType,
    std::string& error)
{
    if (expression->tokenKind() == lexer::TokenKind::QuestionQuestionEqual)
    {
        return lowerNullCoalescingAssignmentExpression(
            expression, memberOrdinal, addressContext, error);
    }

    if (expression->tokenKind() == lexer::TokenKind::Equal &&
        expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::TupleExpression)
    {
        return lowerTupleDeconstructionAssignmentExpression(
            expression, memberOrdinal, addressContext, error);
    }

    const MethodEventBinding* stage42Event = eventBinding(expression);
    if (stage42Event != nullptr &&
        (stage42Event->kind == MethodEventOperationKind::Add ||
         stage42Event->kind == MethodEventOperationKind::Remove))
    {
        return lowerEventAssignmentExpression(
            expression, memberOrdinal, addressContext, stage42Event, error);
    }

    const MethodCallBinding* setterCall = callBinding(expression);
    if (setterCall != nullptr && setterCall->resolution == MethodCallResolutionKind::Resolved &&
        setterCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *setterCall->selectedMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& setter = memberModel_->members()[*setterCall->selectedMemberOrdinal];
        if ((setter.isPropertyAccessor || setter.isIndexerAccessor) && setter.isSetter)
        {
            return lowerSetterAssignmentExpression(
                expression, memberOrdinal, addressContext, typeBinding, setterCall, setter, error);
        }
    }

    // NS304-T08-F02: an extension-property assignment is a synthetic namespace setter call
    // recorded under the assignment node (`set_Name(receiver, value)`). Emit it as a
    // StaticCall whose first argument is the receiver and whose last argument is the RHS.
    const NamespaceFunctionCallBinding* namespaceSetter =
        namespaceFunctionCallBinding(expression);
    if (namespaceSetter != nullptr &&
        namespaceSetter->resolution == MethodCallResolutionKind::Resolved &&
        namespaceSetter->selectedFunctionOrdinal.has_value() &&
        namespaceFunctionModel_ != nullptr &&
        *namespaceSetter->selectedFunctionOrdinal < namespaceFunctionModel_->functions().size())
    {
        return lowerNamespaceSetterAssignmentExpression(
            expression, memberOrdinal, addressContext, namespaceSetter, error);
    }

    // NS304-T10-F05/F06: the receiver remains the first child so LLVM can branch before
    // lowering the RHS. booleanValue carries the semantic owning-destination contract.
    if (expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
    {
        return lowerNullConditionalMemberAssignmentExpression(
            expression, memberOrdinal, resultType, error);
    }

    // NS304-T10-F05: receiver, index, and value remain separate children so only the
    // receiver is evaluated before the null branch.
    if (expression->tokenKind() == lexer::TokenKind::Equal &&
        expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
    {
        return lowerNullConditionalIndexAssignmentExpression(
            expression, memberOrdinal, resultType, error);
    }

    auto node = createOrdinaryAssignmentNode(expression, resultType);
    if (node->operation != lexer::TokenKind::Equal)
    {
        return lowerCompoundAssignmentExpression(
            expression, memberOrdinal, addressContext, resultType, std::move(node), error);
    }
    return lowerSimpleAssignmentExpression(
        expression, memberOrdinal, std::move(node), error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerMoveExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (addressContext)
    {
        error = "Stage 49 move value is not addressable";
        return nullptr;
    }
    const MethodMoveBinding* move = moveBinding(expression);
    if (move == nullptr)
    {
        error = "Stage 49 move expression has no semantic move binding";
        return nullptr;
    }
    if (move->sourceIsArrayElement)
    {
        // Stage 52: move(items[i]) extracts an owned element out of native-array
        // backing storage (needed by Queue<T>.Dequeue()/Stack<T>.Pop()). Reuse the
        // existing IndexExpression address-lowering path (ArrayElementAddress) to
        // compute the slot's address, then wrap it in a dedicated node so LLVM
        // lowering can load the value and, only for reference-like elements,
        // null out the slot to prevent a later double-free/double-use. Value-typed
        // elements (int, bool, a plain struct, ...) have no ownership to transfer,
        // so the load degrades to a harmless plain read with no null-store.
        auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
        if (address == nullptr) return nullptr;
        auto node = makeNode(ProjectOopHirNodeKind::MoveArrayElement, expression, move->typeKey);
        node->booleanValue = move->clearSourceStorage;
        node->children.push_back(std::move(address));
        return node;
    }
    if (move->sourceIsOwnedField)
    {
        // Bloque 1: move(holder.slot) / move(slot) -- take the owner OUT of an `owned`
        // field. Same address-shaped contract as the array-element case above: compute
        // the slot's address here, load-and-null in the backend. The null-store is what
        // makes the container's own discharge (destruction entry, overwrite release) see
        // an empty slot instead of double-freeing the value that just left.
        auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
        if (address == nullptr) return nullptr;
        auto node = makeNode(ProjectOopHirNodeKind::MoveOwnedField, expression, move->typeKey);
        node->booleanValue = move->clearSourceStorage;
        node->children.push_back(std::move(address));
        return node;
    }
    if (move->sourceIsIteratorCurrent)
    {
        // NS304-T07-F04: move(it.Current) takes the move-only item out of the iterator's
        // item slot (field 2). The child is the iterator's ADDRESS; the backend loads the
        // item slot and clears it so the Drop never double-frees the transferred value.
        auto iterator = lowerExpression(expression->first()->first(), memberOrdinal, true, error);
        if (iterator == nullptr) return nullptr;
        auto node = makeNode(ProjectOopHirNodeKind::MoveIteratorCurrent, expression, move->typeKey);
        node->booleanValue = move->clearSourceStorage;
        node->children.push_back(std::move(iterator));
        return node;
    }
    auto node = makeNode(ProjectOopHirNodeKind::MoveValue, expression, move->typeKey);
    node->localOrdinal = move->sourceLocalOrdinal;
    node->parameterOrdinal = move->sourceParameterOrdinal;
    // Stage 52: whether the source is actually reference-like (needs the load+null-store
    // treatment) or a plain value type closed over a generic T -- see
    // MethodMoveBinding::clearSourceStorage's comment and the MoveValue LLVM
    // lowering case for the bug this fixes.
    node->booleanValue = move->clearSourceStorage;
    if (node->localOrdinal.has_value() == node->parameterOrdinal.has_value())
    {
        error = "Stage 50.8 move value must identify exactly one owner source";
        return nullptr;
    }
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerResultPropagationExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const std::string& resultType,
    std::string& error)
{
    if (addressContext)
    {
        error = "postfix '?' produces a value, not addressable storage";
        return nullptr;
    }
    const MethodResultPropagationBinding* binding =
        resultPropagationBinding(expression);
    const MethodStructuredExitBinding* exit = structuredExitBinding(expression);
    if (binding == nullptr || exit == nullptr ||
        exit->kind != MethodStructuredExitKind::Return)
    {
        error = "T07-F01 Result propagation has no complete semantic binding";
        return nullptr;
    }
    auto operand = lowerExpression(
        binding->operandSyntax, memberOrdinal, false, error);
    if (operand == nullptr) return nullptr;

    auto node = makeNode(
        ProjectOopHirNodeKind::ResultPropagation, expression, resultType);
    node->resultSourceCarrierTypeKey = binding->sourceCarrierTypeKey;
    node->resultSourceErrorTypeKey = binding->sourceErrorTypeKey;
    node->resultReturnCarrierTypeKey = binding->returnCarrierTypeKey;
    node->resultReturnErrorTypeKey = binding->returnErrorTypeKey;
    node->resultSuccessTag = binding->sourceSuccessTag;
    node->resultErrorTag = binding->sourceErrorTag;
    node->resultReturnErrorTag = binding->returnErrorTag;
    node->resultSuccessTestMemberOrdinal =
        binding->sourceSuccessTestMemberOrdinal;
    node->resultSuccessExtractorMemberOrdinal =
        binding->sourceSuccessExtractorMemberOrdinal;
    node->resultErrorExtractorMemberOrdinal =
        binding->sourceErrorExtractorMemberOrdinal;
    node->resultErrorFactoryMemberOrdinal =
        binding->returnErrorFactoryMemberOrdinal;
    node->resultErrorConversionMemberOrdinal =
        binding->errorConversionMemberOrdinal;
    node->cleanupRegionOrdinals = exit->cleanupRegionOrdinals;
    node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;
    node->children.push_back(std::move(operand));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerLambdaCreationExpressionImplCase(
    const ast::AstNode* expression,
    const MethodLambdaBinding* stage43Lambda,
    std::string& error)
{
    auto node = makeNode(ProjectOopHirNodeKind::LambdaCreate, expression, stage43Lambda->delegateTypeKey);
    node->elementTypeKey = stage43Lambda->delegateTypeKey;
    node->stringValue = stage43Lambda->generatedName;
    node->integerValue = stage43Lambda->lambdaOrdinal;
    for (const MethodLambdaCapture& capture : stage43Lambda->captures)
    {
        std::unique_ptr<ProjectOopHirNode> value;
        switch (capture.sourceKind)
        {
            case MethodLambdaCaptureSourceKind::OriginalLocal:
            case MethodLambdaCaptureSourceKind::ParentLocal:
                if (capture.sourceOrdinal.has_value())
                {
                    value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                    value->localOrdinal = capture.sourceOrdinal;
                }
                break;
            case MethodLambdaCaptureSourceKind::OriginalParameter:
            case MethodLambdaCaptureSourceKind::ParentParameter:
                if (capture.sourceOrdinal.has_value())
                {
                    value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                    value->parameterOrdinal = capture.sourceOrdinal;
                }
                break;
            case MethodLambdaCaptureSourceKind::OriginalThis:
                value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                break;
            case MethodLambdaCaptureSourceKind::ParentCapture:
                if (capture.sourceOrdinal.has_value())
                {
                    value = makeNode(
                        ProjectOopHirNodeKind::ClosureCaptureCellReference,
                        expression,
                        capture.typeKey);
                    value->integerValue = *capture.sourceOrdinal;
                }
                break;
        }
        if (value == nullptr)
        {
            error = "Stage 43 lambda capture source is unavailable";
            return nullptr;
        }
        value->captureMode = capture.mode;
        value->booleanValue = capture.clearMovedSource;
        node->children.push_back(std::move(value));
    }
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDelegateCreationExpressionImplCase(
    const ast::AstNode* expression,
    const MethodDelegateBinding* stage42Delegate,
    std::string& error)
{
    if (!hasValidDelegateCreationTarget(*stage42Delegate))
    {
        error = "Stage 42 delegate creation has no selected callable";
        return nullptr;
    }
    std::unique_ptr<ProjectOopHirNode> receiver;
    if (!stage42Delegate->staticTarget)
    {
        if (stage42Delegate->ownsReceiver &&
            (stage42Delegate->receiverSourceLocalOrdinal.has_value() ||
             stage42Delegate->receiverSourceParameterOrdinal.has_value()))
        {
            receiver = makeNode(
                ProjectOopHirNodeKind::MoveValue,
                stage42Delegate->receiverSyntax,
                stage42Delegate->receiverTypeKey);
            receiver->localOrdinal = stage42Delegate->receiverSourceLocalOrdinal;
            receiver->parameterOrdinal = stage42Delegate->receiverSourceParameterOrdinal;
            receiver->booleanValue = true;
        }
        else
        {
            receiver = lowerImplicitReceiver(
                stage42Delegate->receiverSyntax,
                stage42Delegate->receiverTypeKey,
                error);
        }
        if (receiver == nullptr) return nullptr;
    }
    return finishDelegateCreation(
        expression, *stage42Delegate, std::move(receiver));
}


// ID 82. `preloweredLast`, cuando no es nulo, SUSTITUYE la bajada del ultimo argumento. Tiene
// un solo llamador: la asignacion compuesta sobre propiedad e indexador, donde el valor que
// recibe el setter no es una expresion de la fuente sino `get_X(...) op derecho`, un arbol que
// hay que construir en el sitio. Todo lo demas --receptor, conversiones seleccionadas,
// borrows-- sigue siendo el mismo codigo para todas las llamadas resueltas, que es justamente
// el motivo de entrar por aqui en vez de armar el nodo de llamada a mano.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerResolvedCall(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error,
    const MethodExpressionTypeBinding* typeBinding,
    const MethodCallBinding* binding,
    const std::vector<const ast::AstNode*>& argumentSyntaxes,
    std::unique_ptr<ProjectOopHirNode> preloweredLast,
    bool nullConditional)
{
    return lowerBoundCall(
        expression,
        memberOrdinal,
        addressContext,
        error,
        typeBinding,
        binding,
        argumentSyntaxes,
        std::move(preloweredLast),
        nullConditional);
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEventBacking(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error,
    const MethodEventBinding* binding,
    bool address)
{
    return lowerEventBackingField(expression, memberOrdinal, error, binding, address);
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerLiteralExpressionImplCase(
    const ast::AstNode* expression,
    bool addressContext,
    const std::string& resultType,
    std::string& error)
{
    switch (expression->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, resultType);
            node->integerValue = expression->integerValue();
            return node;
        }
        case ast::AstNodeKind::FloatingLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, resultType);
            node->floatingValue = expression->floatingValue();
            return node;
        }
        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::NameOfExpression:
        {
            if (addressContext)
            {
                error = "Stage 41 string literals are immutable references and are not addressable values";
                return nullptr;
            }
            if (expression->tokenKind() == lexer::TokenKind::Utf8StringLiteral)
            {
                // NS304-T10-F01: u8"..." is a ReadOnlySpan<byte> over the literal's UTF-8 bytes.
                const MethodCoreBinding* binding = coreBinding(expression);
                const std::string spanKey = binding != nullptr
                    ? binding->resultTypeKey : std::string{};
                auto node = makeNode(
                    ProjectOopHirNodeKind::Utf8StringLiteral, expression,
                    spanKey.empty() ? "builtin:byte" : spanKey);
                node->stringValue = expression->text();
                node->elementTypeKey = "builtin:byte";
                node->integerValue = node->stringValue.size();
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            node->stringValue = expression->text();
            return node;
        }
        case ast::AstNodeKind::BooleanLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::BooleanConstant, expression, "builtin:bool");
            node->booleanValue = expression->tokenKind() == lexer::TokenKind::KwTrue;
            return node;
        }
        case ast::AstNodeKind::NullLiteralExpression:
            return makeNode(ProjectOopHirNodeKind::NullConstant, expression, "builtin:null");
        case ast::AstNodeKind::DefaultValueExpression:
        {
            // Stage 52: default(T) -- the resolved concrete type (already monomorphized, if T
            // was a generic parameter -- see ProjectMethodBodyModel's DefaultValueExpression
            // case) was recorded during semantic analysis; LLVM lowering builds the all-zero-bits
            // value of that EXACT type (see the DefaultValue case in ProjectOopLlvmLowerer.cpp),
            // which is uniformly correct for every T: 0/false/'\0' for a numeric primitive, a
            // null pointer for a reference type or string, an all-zero-fields instance for a
            // value-typed struct.
            if (addressContext)
            {
                error = "Stage 52 default(T) value is not addressable";
                return nullptr;
            }
            const MethodExpressionTypeBinding* type = expressionType(expression);
            if (type == nullptr)
            {
                error = "Stage 52 default(T) expression has no resolved semantic type";
                return nullptr;
            }
            return makeNode(ProjectOopHirNodeKind::DefaultValue, expression, type->typeKey);
        }
        case ast::AstNodeKind::MatchValueExpression:
        {
            if (addressContext)
            {
                auto node = makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
                node->referenceResult = true;
                return node;
            }
            return makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
        }
        default:
            return nullptr;
    }
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerOverflowPolicyExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    auto operand = lowerExpression(expression->first(), memberOrdinal, addressContext, error);
    if (operand == nullptr) return nullptr;
    auto node = makeNode(
        ProjectOopHirNodeKind::OverflowPolicy, expression, operand->typeKey);
    node->stringValue = expression->auxiliaryText();
    node->children.push_back(std::move(operand));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerConditionalExpressionImplCase(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (addressContext)
    {
        error = "a conditional expression produces a value, not addressable storage";
        return nullptr;
    }
    auto condition = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (condition == nullptr) return nullptr;
    auto whenTrue = lowerExpression(expression->second(), memberOrdinal, false, error);
    if (whenTrue == nullptr) return nullptr;
    auto whenFalse = lowerExpression(expression->third(), memberOrdinal, false, error);
    if (whenFalse == nullptr) return nullptr;
    return buildConditionalExpression(
        expression,
        std::move(condition),
        std::move(whenTrue),
        std::move(whenFalse));
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionImpl(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (expression == nullptr)
    {
        error = "null expression reached Stage 34 OOP HIR";
        return nullptr;
    }
    const MethodLambdaBinding* stage43Lambda = lambdaBinding(expression);
    if (stage43Lambda != nullptr && expression->kind() == ast::AstNodeKind::LambdaExpression)
    {
        return lowerLambdaCreationExpressionImplCase(expression, stage43Lambda, error);
    }

    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    if (stage42Delegate != nullptr && stage42Delegate->kind == MethodDelegateOperationKind::Create)
    {
        return lowerDelegateCreationExpressionImplCase(expression, stage42Delegate, error);
    }

    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";

    switch (expression->kind())
    {
        case ast::AstNodeKind::LoopExpression:
            return lowerExpressionWithStatementFrame(
                expression, memberOrdinal, addressContext, nullptr, error);
        case ast::AstNodeKind::ThisExpression:
        {
            return lowerThisExpressionImplCase(
                expression, memberOrdinal, addressContext, resultType, error);
        }

        case ast::AstNodeKind::NullConditionalMemberExpression:
        {
            return lowerNullConditionalMemberExpressionImplCase(
                expression, memberOrdinal, addressContext, typeBinding, resultType, error);
        }

        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        {
            return lowerNameExpressionImplCase(
                expression, memberOrdinal, addressContext, error);
        }

        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        case ast::AstNodeKind::FloatingLiteralExpression:
        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::NameOfExpression:
        case ast::AstNodeKind::BooleanLiteralExpression:
        case ast::AstNodeKind::NullLiteralExpression:
        case ast::AstNodeKind::DefaultValueExpression:
            return lowerLiteralExpressionImplCase(
                expression, addressContext, resultType, error);

        case ast::AstNodeKind::OverflowPolicyExpression:
            return lowerOverflowPolicyExpressionImplCase(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::InterpolatedStringExpression:
            return lowerInterpolatedStringExpressionImplCase(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
        {
            return lowerUnaryExpressionImplCase(
                expression, memberOrdinal, addressContext, resultType, error);
        }

        case ast::AstNodeKind::NullCoalescingExpression:
        {
            return lowerNullCoalescingExpressionImplCase(
                expression, memberOrdinal, addressContext, resultType, error);
        }

        case ast::AstNodeKind::MatchValueExpression:
            return lowerLiteralExpressionImplCase(
                expression, addressContext, resultType, error);

        case ast::AstNodeKind::MatchExpression:
            return lowerExpressionWithStatementFrame(
                expression, memberOrdinal, addressContext, nullptr, error);

        case ast::AstNodeKind::MatchArmExpression:
            error = "a match arm reached HIR outside its match expression";
            return nullptr;

        case ast::AstNodeKind::ConditionalExpression:
            return lowerConditionalExpressionImplCase(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::BinaryExpression:
        {
            return lowerBinaryExpressionImplCase(
                expression, memberOrdinal, addressContext, error);
        }

        case ast::AstNodeKind::IndexExpression:
        case ast::AstNodeKind::NullConditionalIndexExpression:
        {
            return lowerIndexExpressionImplCase(
                expression, memberOrdinal, addressContext, typeBinding, error);
        }

        case ast::AstNodeKind::AssignmentExpression:
        {
            return lowerAssignmentExpressionImplCase(
                expression, memberOrdinal, addressContext, typeBinding, resultType, error);
        }

        case ast::AstNodeKind::MoveExpression:
        {
            return lowerMoveExpressionImplCase(
                expression, memberOrdinal, addressContext, error);
        }

        case ast::AstNodeKind::ResultPropagationExpression:
        {
            return lowerResultPropagationExpressionImplCase(
                expression, memberOrdinal, addressContext, resultType, error);
        }

        case ast::AstNodeKind::CallExpression:
            return lowerCallExpression(expression, memberOrdinal, addressContext, error);

        // CC1 fase 4B -- `stackalloc T[n]`. Un alloca de arreglo y nada mas: sin cabecera de
        // longitud, sin comprobacion de rango, sin liberacion. Es el nivel crudo, igual que
        // `p[i]`, y la validacion de que el elemento puede vivir ahi ya la hizo el modelo
        // semantico (sin destructor y sin campos `owned`).
        case ast::AstNodeKind::StackallocExpression:
        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::CollectionExpression:
            return lowerArrayConstructionExpression(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::ObjectCreationExpression:
        case ast::AstNodeKind::PlacementConstructionExpression:
            return lowerObjectConstructionExpression(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::TupleExpression:
        case ast::AstNodeKind::AggregateInitializerExpression:
        case ast::AstNodeKind::WithExpression:
            return lowerAggregateOrLayoutExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::DeleteExpression:
        case ast::AstNodeKind::DestroyExpression:
            return lowerDestructionExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::TypeTestExpression:
        case ast::AstNodeKind::SafeCastExpression:
        case ast::AstNodeKind::CastExpression:
            return lowerConversionExpression(
                expression, memberOrdinal, addressContext, error, typeBinding, resultType);

        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        case ast::AstNodeKind::OffsetofExpression:
            return lowerAggregateOrLayoutExpression(expression, memberOrdinal, error);
        // ID 80, CERRADA. Aqui habia un rechazo con mensaje propio para `p->campo`: era la ultima
        // forma de expresion con la mitad delantera del pipeline presente y la trasera ausente,
        // despues de sizeof, stackalloc, raw new y la aritmetica de punteros.
        //
        // Se cerro donde tocaba, que no era aqui: el parser convierte `p->x` en `(*p).x` (ver
        // Parser::parsePostfixExpression), asi que este nodo ya no se construye y este caso ya no
        // se alcanza. Anadir en su lugar un lowering propio habria significado una SEGUNDA ruta de
        // acceso a miembro que mantener de acuerdo con la de `.` -- que es como se abrio el hueco.
        //
        // NOTA sobre como se hizo el inventario que la encontro: un barrido de las 30 formas de
        // expresion del AST contra los `case` de esta funcion daba TAMBIEN `LambdaExpression`. Es
        // un falso positivo del barrido: las lambdas se bajan por binding ANTES del switch, no por
        // un `case`. Comprobado antes de escribirlo -- el barrido mide lo que mide, no lo que uno
        // quiere que mida.

        case ast::AstNodeKind::AwaitExpression:
            error = "await expression escaped iterative Stage 34 OOP HIR lowering";
            return nullptr;

        default:
            error = "unsupported Stage 34 OOP expression kind: " +
                std::string(ast::astNodeKindName(expression->kind()));
            return nullptr;
    }
}


// NSC-LIMIT-008. Is this name actually a GETTER CALL?
//
// `source.Current` and `table[key]` are written like storage and parse like storage, but if the
// semantic model bound them to a property or indexer accessor they are calls: they RETURN a value,
// and that value has no home. Syntax cannot tell the two apart -- `point.X` is a field on one type
// and a property on another, and the difference is only known after member resolution.
//
// Two places need this answer and they used to disagree. lowerExpression asked it (the
// MemberExpression case, which rejects `addressContext` for a getter) while isNaturallyAddressable
// did not, and answered `true` for every MemberExpression. That disagreement is the whole defect:
// `source.Current.Read()` classified the getter as addressable, so lowerReceiver never entered its
// TemporaryAddress path and the struct method received the address of something that was never
// stored.
const MethodCallBinding* ProjectOopHirModel::accessorGetterBinding(
    const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr || memberModel_ == nullptr) return nullptr;
    switch (expression->kind())
    {
        // The three spellings a getter can wear. `Current` unqualified inside its own class is an
        // IdentifierExpression, `source.Current` a MemberExpression, `table[key]` an
        // IndexExpression -- and all three are calls once the model has resolved them.
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::NullConditionalMemberExpression:
        case ast::AstNodeKind::IndexExpression:
            break;
        default:
            return nullptr;
    }
    const MethodCallBinding* accessorCall = callBinding(expression);
    if (accessorCall == nullptr ||
        accessorCall->resolution != MethodCallResolutionKind::Resolved ||
        !accessorCall->selectedMemberOrdinal.has_value() ||
        *accessorCall->selectedMemberOrdinal >= memberModel_->members().size())
    {
        // No binding at all is the ORDINARY case, not an error: a real field, a real array element
        // and a real fixed-array element never produce one. They keep their own address.
        return nullptr;
    }
    const ClassMemberSymbol& selected = memberModel_->members()[*accessorCall->selectedMemberOrdinal];
    // PROPIEDAD *O* INDEXADOR. `isIndexerAccessor` es una bandera distinta de `isPropertyAccessor`,
    // no un caso suyo, y mirar solo la primera dejaba fuera `obj[i].MetodoDeStruct()`: el
    // IndexExpression entraba por el switch de arriba, resolvia a un getter de indexador, y este
    // filtro lo dejaba pasar como si fuera almacenamiento. Exactamente el mismo defecto que este
    // ID cierra, en la otra mitad de la sintaxis.
    if (!selected.isPropertyAccessor && !selected.isIndexerAccessor) return nullptr;
    if (!selected.isGetter) return nullptr;
    return accessorCall;
}

// C6 "struct members": does this expression denote storage that already exists? An identifier, a
// field of one, an element of one, and a dereference all do -- and a parenthesized form of any of
// them does too. A call, an operator, a conversion or a `new` does not: its result is a value with
// no home. Only the second group needs a temporary made for it.
//
// NSC-LIMIT-008 added the one case where syntax lies: a property or indexer getter is written like
// a field or an element and is a call. See accessorGetterBinding.
bool ProjectOopHirModel::isNaturallyAddressable(const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr) return false;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::IndexExpression:
        {
            // A real field or element is storage. An ordinary accessor returns a value with no
            // home, while a ref-returning accessor deliberately returns existing storage.
            const MethodCallBinding* accessor = accessorGetterBinding(expression);
            if (accessor == nullptr) return true;
            return accessor->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *accessor->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*accessor->selectedMemberOrdinal].isReferenceReturn;
        }
        case ast::AstNodeKind::ThisExpression:
        case ast::AstNodeKind::BaseExpression:
        case ast::AstNodeKind::MatchValueExpression:
            return true;
        case ast::AstNodeKind::UnaryExpression:
            // `*p` is storage; `-v` is a value.
            return expression->tokenKind() == lexer::TokenKind::Star;
        default:
            return false;
    }
}


}
