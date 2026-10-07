#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace nativesharp::frontend
{
namespace
{
bool lifetimeOutlives(
    const ClassMemberSymbol& callable,
    std::string_view longer,
    std::string_view shorter)
{
    if (longer == shorter || longer == "static") return true;
    std::unordered_set<std::string> visited;
    std::vector<std::string> pending{std::string(longer)};
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(current).second) continue;
        for (const auto& [declaredLonger, declaredShorter] :
             callable.lifetimeOutlivesConstraints)
        {
            if (declaredLonger != current) continue;
            if (declaredShorter == shorter) return true;
            pending.push_back(declaredShorter);
        }
    }
    return false;
}

int unsafeRank(MethodEscapeOriginKind kind) noexcept
{
    switch (kind)
    {
        case MethodEscapeOriginKind::VariableLengthArray: return 4;
        case MethodEscapeOriginKind::CurrentFrameLocal: return 3;
        case MethodEscapeOriginKind::CurrentFrameParameter: return 2;
        case MethodEscapeOriginKind::CurrentReceiver: return 1;
        case MethodEscapeOriginKind::ExternalStorage: return 0;
    }
    return 0;
}
}

ProjectReferenceEscapeEvaluator::ProjectReferenceEscapeEvaluator(
    const ProjectMethodBodyModel& model,
    std::size_t scopeOrdinal) noexcept
    : model_(model),
      scopeOrdinal_(scopeOrdinal),
      callableOrdinal_(scopeOrdinal < model.scopes_.size()
          ? model.scopes_[scopeOrdinal].methodMemberOrdinal
          : std::numeric_limits<std::size_t>::max())
{
}

MethodEscapeOrigin ProjectReferenceEscapeEvaluator::evaluate(
    const ast::AstNode* expression) const
{
    ++model_.escapePropagationStatistics_.referenceOriginEvaluations;
    const ast::AstNode* next = expression;
    MethodEscapeOrigin result;
    std::vector<ReferenceEscapeEvaluationFrame> continuations;
    while (true)
    {
        if (next != nullptr)
        {
            const ast::AstNode* current = next;
            next = nullptr;
            startNode(current, next, result, continuations);
            if (next != nullptr) continue;
        }
        if (continuations.empty()) return result;
        resume(next, result, continuations);
    }
}

void ProjectReferenceEscapeEvaluator::startNode(
    const ast::AstNode* expression,
    const ast::AstNode*& next,
    MethodEscapeOrigin& result,
    std::vector<ReferenceEscapeEvaluationFrame>& continuations) const
{
    result = {};
    if (expression == nullptr) return;
    switch (expression->kind())
    {
        case ast::AstNodeKind::ThisExpression:
        case ast::AstNodeKind::BaseExpression:
            result = {MethodEscapeOriginKind::CurrentReceiver, std::nullopt, std::nullopt, "this"};
            return;
        case ast::AstNodeKind::IdentifierExpression:
            startIdentifier(expression, result);
            return;
        case ast::AstNodeKind::MatchExpression:
            startMatch(expression, next, result, continuations);
            return;
        case ast::AstNodeKind::MemberExpression:
            startMember(expression, next, result, continuations);
            return;
        case ast::AstNodeKind::IndexExpression:
            startIndex(expression, next, result);
            return;
        case ast::AstNodeKind::CallExpression:
            startCall(expression, next, continuations);
            return;
        default:
            break;
    }
    if (expression->kind() == ast::AstNodeKind::UnaryExpression &&
        expression->tokenKind() == lexer::TokenKind::Star)
    {
        startDereference(expression, result);
        return;
    }
    next = expression->first();
}

void ProjectReferenceEscapeEvaluator::startIdentifier(
    const ast::AstNode* expression,
    MethodEscapeOrigin& result) const
{
    const std::optional<ProjectMethodBodyModel::ScopeName> name =
        model_.lookupScopeName(scopeOrdinal_, expression->text());
    if (!name.has_value())
    {
        const MethodNameBinding* binding = model_.nameBinding(expression, callableOrdinal_);
        if (binding != nullptr && binding->kind == MethodNameBindingKind::ImplicitField)
            result = {MethodEscapeOriginKind::CurrentReceiver, std::nullopt, std::nullopt, "this"};
        else if (binding != nullptr && binding->kind == MethodNameBindingKind::StaticField)
            result = {MethodEscapeOriginKind::ExternalStorage, std::nullopt, std::nullopt, "static"};
        return;
    }
    if (name->kind == ProjectMethodBodyModel::ScopeNameKind::Local &&
        name->semanticOrdinal < model_.locals_.size())
    {
        const MethodLocalSymbol& local = model_.locals_[name->semanticOrdinal];
        if (local.variableLengthArrayRootLocalOrdinal.has_value())
        {
            const std::size_t root = *local.variableLengthArrayRootLocalOrdinal;
            result = {MethodEscapeOriginKind::VariableLengthArray,
                local.variableLengthArrayRootLocalOrdinal,
                root < model_.locals_.size()
                    ? std::optional<std::size_t>(model_.locals_[root].scopeOrdinal) : std::nullopt, {}};
        }
        else if (local.isReference) result = local.referenceEscapeOrigin;
        else result = {MethodEscapeOriginKind::CurrentFrameLocal, name->semanticOrdinal,
            local.scopeOrdinal, {}};
        return;
    }
    if (name->kind != ProjectMethodBodyModel::ScopeNameKind::Parameter) return;
    const ClassMemberSymbol* method = model_.methodSymbol(callableOrdinal_);
    if (method == nullptr || name->semanticOrdinal >= method->parameters.size()) return;
    const MethodParameterSymbol& parameter = method->parameters[name->semanticOrdinal];
    const MethodEscapeOriginKind kind = parameter.passKind == ParameterPassKind::Ref ||
        parameter.passKind == ParameterPassKind::Out
        ? MethodEscapeOriginKind::ExternalStorage
        : MethodEscapeOriginKind::CurrentFrameParameter;
    result = {kind, name->semanticOrdinal, rootScopeOrdinal(),
        kind == MethodEscapeOriginKind::ExternalStorage ? parameter.lifetimeName : std::string{}};
}

void ProjectReferenceEscapeEvaluator::startMatch(
    const ast::AstNode* expression,
    const ast::AstNode*& next,
    MethodEscapeOrigin& result,
    std::vector<ReferenceEscapeEvaluationFrame>& continuations) const
{
    ReferenceEscapeEvaluationFrame frame;
    frame.kind = ReferenceEscapeContinuationKind::MatchArms;
    frame.syntax = expression;
    if (!scheduleNextMatchArm(frame, next))
    {
        result = {};
        return;
    }
    pushContinuation(std::move(frame), continuations);
}

void ProjectReferenceEscapeEvaluator::startMember(
    const ast::AstNode* expression,
    const ast::AstNode*& next,
    MethodEscapeOrigin& result,
    std::vector<ReferenceEscapeEvaluationFrame>& continuations) const
{
    const ast::AstNode* receiver = expression->first();
    if (receiver != nullptr &&
        (receiver->kind() == ast::AstNodeKind::ThisExpression ||
         receiver->kind() == ast::AstNodeKind::BaseExpression))
    {
        result = {MethodEscapeOriginKind::CurrentReceiver, std::nullopt, std::nullopt, "this"};
        return;
    }
    const std::string_view receiverType = typeKeyOf(receiver);
    if (model_.typeKeyIsStruct(receiverType))
    {
        next = receiver;
        return;
    }
    if (!ProjectTypeConversion::isPointer(receiverType)) return;

    ReferenceEscapeEvaluationFrame frame;
    frame.kind = ReferenceEscapeContinuationKind::PointerMember;
    frame.syntax = receiver;
    pushContinuation(std::move(frame), continuations);
    next = receiver;
}

void ProjectReferenceEscapeEvaluator::startIndex(
    const ast::AstNode* expression,
    const ast::AstNode*& next,
    MethodEscapeOrigin& result) const
{
    if (const ClassMemberSymbol* callee = resolvedCallee(expression);
        callee != nullptr && callee->isReferenceReturn)
    {
        const MethodCallBinding* call = resolvedCall(expression);
        if (callee->returnLifetimeName == "this") next = call->receiverSyntax;
        else if (callee->returnLifetimeName != "static")
        {
            ReferenceEscapeEvaluationFrame frame;
            frame.kind = ReferenceEscapeContinuationKind::CallArguments;
            frame.syntax = expression;
            scheduleNextCallArgument(frame, next);
        }
        return;
    }
    const std::optional<std::size_t> vla =
        model_.variableArrayRootLocalOfExpression(expression->first(), scopeOrdinal_);
    if (vla.has_value())
    {
        result = {MethodEscapeOriginKind::VariableLengthArray, vla,
            *vla < model_.locals_.size()
                ? std::optional<std::size_t>(model_.locals_[*vla].scopeOrdinal) : std::nullopt, {}};
        return;
    }
    const std::string_view baseType = typeKeyOf(expression->first());
    if (ProjectMemberModel::fixedArrayElementTypeKey(baseType).has_value() ||
        ProjectMemberModel::flexibleArrayElementTypeKey(baseType).has_value())
        next = expression->first();
}

void ProjectReferenceEscapeEvaluator::startCall(
    const ast::AstNode* expression,
    const ast::AstNode*& next,
    std::vector<ReferenceEscapeEvaluationFrame>& continuations) const
{
    const ClassMemberSymbol* callee = resolvedCallee(expression);
    if (callee == nullptr || !callee->isReferenceReturn) return;
    const MethodCallBinding* call = resolvedCall(expression);
    if (callee->returnLifetimeName == "this")
    {
        next = call->receiverSyntax;
        return;
    }
    if (callee->returnLifetimeName == "static") return;
    ReferenceEscapeEvaluationFrame frame;
    frame.kind = ReferenceEscapeContinuationKind::CallArguments;
    frame.syntax = expression;
    if (!scheduleNextCallArgument(frame, next)) return;
    pushContinuation(std::move(frame), continuations);
}

void ProjectReferenceEscapeEvaluator::startDereference(
    const ast::AstNode* expression,
    MethodEscapeOrigin& result) const
{
    const std::optional<std::size_t> vla =
        model_.variableArrayRootLocalOfExpression(expression->first(), scopeOrdinal_);
    if (!vla.has_value()) return;
    result = {MethodEscapeOriginKind::VariableLengthArray, vla,
        *vla < model_.locals_.size()
            ? std::optional<std::size_t>(model_.locals_[*vla].scopeOrdinal) : std::nullopt, {}};
}

void ProjectReferenceEscapeEvaluator::resume(
    const ast::AstNode*& next,
    MethodEscapeOrigin& result,
    std::vector<ReferenceEscapeEvaluationFrame>& continuations) const
{
    ReferenceEscapeEvaluationFrame& frame = continuations.back();
    if (frame.kind == ReferenceEscapeContinuationKind::PointerMember)
    {
        if (result.kind == MethodEscapeOriginKind::CurrentFrameParameter)
            result.kind = MethodEscapeOriginKind::ExternalStorage;
        else if (frame.syntax != nullptr &&
                 frame.syntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                 result.kind == MethodEscapeOriginKind::CurrentFrameLocal)
            result = {};
        continuations.pop_back();
        return;
    }
    if (frame.kind == ReferenceEscapeContinuationKind::CallArguments)
    {
        if (result.kind != MethodEscapeOriginKind::ExternalStorage || !result.lifetimeName.empty())
        {
            continuations.pop_back();
            return;
        }
        if (scheduleNextCallArgument(frame, next)) return;
        continuations.pop_back();
        result = {};
        return;
    }
    if (!frame.hasAccumulated || unsafeRank(result.kind) > unsafeRank(frame.accumulated.kind))
        frame.accumulated = result;
    else if (result.kind == frame.accumulated.kind &&
        (result.ordinal != frame.accumulated.ordinal ||
         result.lifetimeName != frame.accumulated.lifetimeName))
    {
        frame.accumulated.ordinal.reset();
        if (result.lifetimeName != frame.accumulated.lifetimeName)
            frame.accumulated.lifetimeName.clear();
    }
    frame.hasAccumulated = true;
    if (scheduleNextMatchArm(frame, next)) return;
    result = frame.accumulated;
    continuations.pop_back();
}

bool ProjectReferenceEscapeEvaluator::scheduleNextMatchArm(
    ReferenceEscapeEvaluationFrame& frame,
    const ast::AstNode*& next) const
{
    const std::span<ast::AstNode* const> arms = frame.syntax->children();
    while (frame.nextIndex < arms.size())
    {
        const ast::AstNode* arm = arms[frame.nextIndex++];
        if (arm == nullptr || arm->third() == nullptr) continue;
        next = arm->third();
        return true;
    }
    return false;
}

bool ProjectReferenceEscapeEvaluator::scheduleNextCallArgument(
    ReferenceEscapeEvaluationFrame& frame,
    const ast::AstNode*& next) const
{
    const MethodCallBinding* call = resolvedCall(frame.syntax);
    const ClassMemberSymbol* callee = resolvedCallee(frame.syntax);
    if (call == nullptr || callee == nullptr) return false;
    const std::span<ast::AstNode* const> arguments = frame.syntax->children();
    while (frame.nextIndex < arguments.size())
    {
        const std::size_t index = frame.nextIndex++;
        const std::size_t parameterIndex = index < call->selectedArgumentParameterOrdinals.size()
            ? call->selectedArgumentParameterOrdinals[index] : index;
        if (parameterIndex >= callee->parameters.size()) continue;
        if (!lifetimeOutlives(*callee, callee->parameters[parameterIndex].lifetimeName,
                callee->returnLifetimeName)) continue;
        next = arguments[index];
        return true;
    }
    return false;
}

void ProjectReferenceEscapeEvaluator::pushContinuation(
    ReferenceEscapeEvaluationFrame frame,
    std::vector<ReferenceEscapeEvaluationFrame>& continuations) const
{
    continuations.push_back(std::move(frame));
    ProjectEscapePropagationStatistics& statistics = model_.escapePropagationStatistics_;
    ++statistics.referenceContinuationFrames;
    statistics.maximumReferenceContinuationDepth = std::max(
        statistics.maximumReferenceContinuationDepth,
        continuations.size());
}

std::string_view ProjectReferenceEscapeEvaluator::typeKeyOf(const ast::AstNode* syntax) const
{
    const MethodExpressionTypeBinding* binding =
        model_.expressionTypeBinding(syntax, callableOrdinal_);
    return binding != nullptr ? std::string_view(binding->typeKey) : std::string_view{};
}

const MethodCallBinding* ProjectReferenceEscapeEvaluator::resolvedCall(
    const ast::AstNode* syntax) const
{
    const MethodCallBinding* binding = model_.callBinding(syntax, callableOrdinal_);
    return binding != nullptr && binding->resolution == MethodCallResolutionKind::Resolved &&
        binding->selectedMemberOrdinal.has_value() ? binding : nullptr;
}

const ClassMemberSymbol* ProjectReferenceEscapeEvaluator::resolvedCallee(
    const ast::AstNode* syntax) const
{
    const MethodCallBinding* call = resolvedCall(syntax);
    return call != nullptr && model_.memberModel_ != nullptr &&
        *call->selectedMemberOrdinal < model_.memberModel_->members().size()
        ? &model_.memberModel_->members()[*call->selectedMemberOrdinal] : nullptr;
}

std::size_t ProjectReferenceEscapeEvaluator::rootScopeOrdinal() const noexcept
{
    std::size_t root = scopeOrdinal_;
    while (root < model_.scopes_.size() && model_.scopes_[root].parentScopeOrdinal.has_value())
        root = *model_.scopes_[root].parentScopeOrdinal;
    return root;
}
}
