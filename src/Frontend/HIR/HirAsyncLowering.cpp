#include "HirLoweringSupport.h"

#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
void ProjectOopHirModel::assignYieldStates(
    ProjectOopHirNode* root,
    std::size_t& nextYieldState)
{
    std::vector<ProjectOopHirNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        ProjectOopHirNode* node = pending.back();
        pending.pop_back();
        if (node->kind == ProjectOopHirNodeKind::YieldValue)
            node->integerValue = ++nextYieldState;
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            if (*child != nullptr) pending.push_back(child->get());
    }
}

void ProjectOopHirModel::rewriteGeneratorThrows(
    ProjectOopHirNode* root,
    std::string_view errorTypeKey,
    std::string_view iteratorKey)
{
    std::vector<std::unique_ptr<ProjectOopHirNode>*> pending;
    if (root != nullptr)
        for (auto child = root->children.rbegin(); child != root->children.rend(); ++child)
            pending.push_back(&*child);
    while (!pending.empty())
    {
        std::unique_ptr<ProjectOopHirNode>* slot = pending.back();
        pending.pop_back();
        if (*slot == nullptr) continue;
        ProjectOopHirNode* child = slot->get();
        if (child->kind == ProjectOopHirNodeKind::Throw)
        {
            auto block = makeNode(ProjectOopHirNodeKind::Block, child->syntax, "builtin:void");
            auto errorField = makeNode(
                ProjectOopHirNodeKind::FutureFieldAddress, child->syntax,
                std::string(errorTypeKey));
            errorField->memberOrdinal = 5;
            errorField->elementTypeKey = iteratorKey;
            auto selfAddress = makeNode(
                ProjectOopHirNodeKind::ParameterValue, child->syntax,
                std::string(iteratorKey) + "*");
            selfAddress->parameterOrdinal = 0;
            errorField->children.push_back(std::move(selfAddress));
            auto errorStore = makeNode(
                ProjectOopHirNodeKind::Assignment, child->syntax, std::string(errorTypeKey));
            errorStore->operation = lexer::TokenKind::Equal;
            errorStore->children.push_back(std::move(errorField));
            errorStore->children.push_back(!child->children.empty()
                ? std::move(child->children[0])
                : makeNode(ProjectOopHirNodeKind::NullConstant, child->syntax,
                    std::string(errorTypeKey)));
            block->children.push_back(std::move(errorStore));
            auto stateField = makeNode(
                ProjectOopHirNodeKind::FutureFieldAddress, child->syntax, "builtin:int");
            stateField->memberOrdinal = 0;
            stateField->elementTypeKey = iteratorKey;
            auto stateSelf = makeNode(
                ProjectOopHirNodeKind::ParameterValue, child->syntax,
                std::string(iteratorKey) + "*");
            stateSelf->parameterOrdinal = 0;
            stateField->children.push_back(std::move(stateSelf));
            auto errored = makeNode(
                ProjectOopHirNodeKind::IntegerConstant, child->syntax, "builtin:int");
            errored->integerValue = ~0ULL - 1ULL;
            auto stateStore = makeNode(
                ProjectOopHirNodeKind::Assignment, child->syntax, "builtin:int");
            stateStore->operation = lexer::TokenKind::Equal;
            stateStore->children.push_back(std::move(stateField));
            stateStore->children.push_back(std::move(errored));
            block->children.push_back(std::move(stateStore));
            auto retFalse = makeNode(
                ProjectOopHirNodeKind::Return, child->syntax, "builtin:bool");
            auto falseValue = makeNode(
                ProjectOopHirNodeKind::BooleanConstant, child->syntax, "builtin:bool");
            falseValue->booleanValue = false;
            retFalse->children.push_back(std::move(falseValue));
            block->children.push_back(std::move(retFalse));
            *slot = std::move(block);
            continue;
        }
        for (auto nested = child->children.rbegin(); nested != child->children.rend(); ++nested)
            pending.push_back(&*nested);
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::rewriteAsyncPollReturns(
    std::unique_ptr<ProjectOopHirNode> body,
    std::size_t selfParameterOrdinal,
    std::string_view resultTypeKey,
    std::string_view futureKey,
    std::string& error,
    bool topLevel)
{
    if (body == nullptr)
    {
        return nullptr;
    }

    (void)error;
    const auto rewriteThrow = [&](std::unique_ptr<ProjectOopHirNode> throwing)
        -> std::unique_ptr<ProjectOopHirNode>
    {
        // NS304-T05-F07: `throw E;` becomes a future error, not a native unwind:
        // { self->error = E; self->state = -2 (error); return; }
        auto block = makeNode(ProjectOopHirNodeKind::Block, throwing->syntax, "builtin:void");
        const auto selfParameter = [&]() -> std::unique_ptr<ProjectOopHirNode>
        {
            auto self = makeNode(
                ProjectOopHirNodeKind::ParameterValue,
                throwing->syntax,
                std::string(futureKey) + "*");
            self->parameterOrdinal = selfParameterOrdinal;
            return self;
        };

        auto errorField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, throwing->syntax,
            throwing->children.empty() ? std::string{} : throwing->children[0]->typeKey);
        errorField->memberOrdinal = 4; // error (pollFn is index 3)
        errorField->elementTypeKey = std::string(futureKey);
        errorField->children.push_back(selfParameter());

        std::unique_ptr<ProjectOopHirNode> errorValue = !throwing->children.empty()
            ? std::move(throwing->children[0])
            : makeNode(ProjectOopHirNodeKind::NullConstant, throwing->syntax, errorField->typeKey);
        auto errorStore = makeNode(
            ProjectOopHirNodeKind::Assignment, throwing->syntax, errorField->typeKey);
        errorStore->operation = lexer::TokenKind::Equal;
        errorStore->children.push_back(std::move(errorField));
        errorStore->children.push_back(std::move(errorValue));
        block->children.push_back(std::move(errorStore));

        auto stateField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, throwing->syntax, "builtin:int");
        stateField->memberOrdinal = 0; // state
        stateField->elementTypeKey = std::string(futureKey);
        stateField->children.push_back(selfParameter());
        auto errored = makeNode(
            ProjectOopHirNodeKind::IntegerConstant, throwing->syntax, "builtin:int");
        errored->integerValue = ~0ULL - 1ULL; // error = -2 (uniform; ready = -1, resume 1..N)
        auto stateStore = makeNode(
            ProjectOopHirNodeKind::Assignment, throwing->syntax, "builtin:int");
        stateStore->operation = lexer::TokenKind::Equal;
        stateStore->children.push_back(std::move(stateField));
        stateStore->children.push_back(std::move(errored));
        block->children.push_back(std::move(stateStore));

        auto synthesizedReturn = makeNode(
            ProjectOopHirNodeKind::Return, throwing->syntax, "builtin:void");
        // NS304-T17-F09: the throw's synthesized `return;` must carry the throw's cleanup regions
        // (owned locals + finally), exactly like rewriteReturn, so they run once on the error path.
        synthesizedReturn->cleanupRegionOrdinals = throwing->cleanupRegionOrdinals;
        synthesizedReturn->cleanupContinuationKind = throwing->cleanupContinuationKind;
        block->children.push_back(std::move(synthesizedReturn));
        return block;
    };

    const auto rewriteReturn = [&](std::unique_ptr<ProjectOopHirNode> ret)
        -> std::unique_ptr<ProjectOopHirNode>
    {
        // `return v;` -> { self->result = v; self->state = -1 (ready); return; }
        auto block = makeNode(ProjectOopHirNodeKind::Block, ret->syntax, "builtin:void");
        const auto selfParameter = [&]() -> std::unique_ptr<ProjectOopHirNode>
        {
            auto self = makeNode(
                ProjectOopHirNodeKind::ParameterValue, ret->syntax, std::string(futureKey) + "*");
            self->parameterOrdinal = selfParameterOrdinal;
            return self;
        };
        auto resultField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, ret->syntax, std::string(resultTypeKey));
        resultField->memberOrdinal = 2; // result
        resultField->elementTypeKey = futureKey;
        resultField->children.push_back(selfParameter());
        std::unique_ptr<ProjectOopHirNode> value = !ret->children.empty()
            ? std::move(ret->children[0])
            : makeNode(ProjectOopHirNodeKind::DefaultValue, ret->syntax, std::string(resultTypeKey));
        // NS304-T07-F06: a Future<void> has no result slot — emitting a store through a void field
        // fails and drops the state store, leaving the poll stuck. Only store a non-void result.
        if (resultTypeKey != "builtin:void")
        {
            auto resultStore = makeNode(
                ProjectOopHirNodeKind::Assignment, ret->syntax, std::string(resultTypeKey));
            resultStore->operation = lexer::TokenKind::Equal;
            resultStore->children.push_back(std::move(resultField));
            resultStore->children.push_back(std::move(value));
            block->children.push_back(std::move(resultStore));
        }
        auto stateField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, ret->syntax, "builtin:int");
        stateField->memberOrdinal = 0; // state
        stateField->elementTypeKey = futureKey;
        stateField->children.push_back(selfParameter());
        auto ready = makeNode(ProjectOopHirNodeKind::IntegerConstant, ret->syntax, "builtin:int");
        ready->integerValue = ~0ULL; // ready = -1 (uniform); pending/resume states are 1..N
        auto stateStore = makeNode(
            ProjectOopHirNodeKind::Assignment, ret->syntax, "builtin:int");
        stateStore->operation = lexer::TokenKind::Equal;
        stateStore->children.push_back(std::move(stateField));
        stateStore->children.push_back(std::move(ready));
        block->children.push_back(std::move(stateStore));
        // NS304-T05-F06: the synthesized `return;` must carry the original return's cleanup
        // regions, so owned locals drop exactly once on completion.
        auto synthesizedReturn = makeNode(
            ProjectOopHirNodeKind::Return, ret->syntax, "builtin:void");
        synthesizedReturn->cleanupRegionOrdinals = ret->cleanupRegionOrdinals;
        synthesizedReturn->cleanupContinuationKind = ret->cleanupContinuationKind;
        block->children.push_back(std::move(synthesizedReturn));
        return block;
    };

    const auto rewriteYield = [&](std::unique_ptr<ProjectOopHirNode> yielding)
        -> std::unique_ptr<ProjectOopHirNode>
    {
        // NS304-T07-F02: `yield return v;` in the async-generator poll -> AsyncYieldValue(v). The
        // poll stores the item + the demand future's result/state; integerValue is the resume state.
        auto node = makeNode(
            ProjectOopHirNodeKind::AsyncYieldValue, yielding->syntax, "builtin:void");
        node->integerValue = yielding->integerValue;
        // NS304-T07-F05: the cancellation path must run the same finally/defer as the terminal
        // return, so carry the yield's cleanup regions.
        node->cleanupRegionOrdinals = yielding->cleanupRegionOrdinals;
        node->cleanupContinuationKind = yielding->cleanupContinuationKind;
        if (!yielding->children.empty() && yielding->children[0] != nullptr)
            node->children.push_back(std::move(yielding->children[0]));
        return node;
    };

    // H02B3: transform terminal nodes through stable owning slots. Replacing a unique_ptr does not
    // invalidate sibling slots, and reverse pushes preserve the old recursive left-to-right order.
    std::vector<std::unique_ptr<ProjectOopHirNode>*> pending;
    pending.push_back(&body);
    while (!pending.empty())
    {
        auto* slot = pending.back();
        pending.pop_back();
        if (*slot == nullptr)
        {
            return nullptr;
        }

        ProjectOopHirNode* node = slot->get();
        if (node->kind == ProjectOopHirNodeKind::Throw)
        {
            *slot = rewriteThrow(std::move(*slot));
            continue;
        }
        if (node->kind == ProjectOopHirNodeKind::Return)
        {
            *slot = rewriteReturn(std::move(*slot));
            continue;
        }
        if (node->kind == ProjectOopHirNodeKind::YieldValue)
        {
            *slot = rewriteYield(std::move(*slot));
            continue;
        }

        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            pending.push_back(&*child);
    }

    // NS304-T07-F06: a TOP-LEVEL body that falls off the end (implicit return) still parks the
    // ready state (state = -1); without it a blocking await polls forever. Nested blocks (e.g. the
    // finally body) must not get the terminal. A rewritten Throw/Return already ends in a Block
    // whose last child is a Return, and a Yield ends in AsyncYieldValue.
    if (topLevel && body->kind == ProjectOopHirNodeKind::Block)
    {
        bool terminal = false;
        if (!body->children.empty())
        {
            const ProjectOopHirNode* last = body->children.back().get();
            if (last->kind == ProjectOopHirNodeKind::AsyncYieldValue)
            {
                terminal = true;
            }
            else if (last->kind == ProjectOopHirNodeKind::Block && !last->children.empty() &&
                     last->children.back()->kind == ProjectOopHirNodeKind::Return)
            {
                terminal = true;
            }
        }
        if (!terminal)
        {
            auto implicitReturn = makeNode(
                ProjectOopHirNodeKind::Return, body->syntax, "builtin:void");
            body->children.push_back(rewriteReturn(std::move(implicitReturn)));
        }
    }
    return body;
}

std::unique_ptr<ProjectOopHirFunction> ProjectOopHirModel::splitAsyncFunction(
    ProjectOopHirFunction& function,
    std::string& error)
{
    if (function.body == nullptr)
    {
        error = "async function has no lowered body to split";
        return nullptr;
    }
    const std::string futureKey = function.returnTypeKey;
    const auto inner = language::futureInnerTypeKey(futureKey);
    if (!inner.has_value())
    {
        error = "async function return type is not a future surface: " + futureKey;
        return nullptr;
    }
    const std::string resultTypeKey(*inner);
    const ast::AstNode* syntax = function.body->syntax;

    auto poll = std::make_unique<ProjectOopHirFunction>();
    poll->memberOrdinal = function.memberOrdinal;
    poll->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    poll->declaringTypeOrdinal = function.declaringTypeOrdinal;
    poll->isStatic = true;
    poll->isAsyncPoll = true;
    poll->inliningPolicy = function.inliningPolicy;
    poll->mangledName = function.mangledName + "$poll";
    poll->debugDisplayName = function.debugDisplayName + "$poll";
    poll->returnTypeKey = "builtin:void";
    poll->errorTypeKey = function.errorTypeKey;
    poll->effects = function.effects;
    poll->returnOwnership = sema::OwnershipKind::None;
    poll->parameterNames.push_back("self");
    poll->parameterTypeKeys.push_back(futureKey + "*");
    poll->parameterPassKinds.push_back(ParameterPassKind::Value);
    poll->parameterOwnerships.push_back(sema::OwnershipKind::None);
    poll->body = rewriteAsyncPollReturns(
        std::move(function.body), 0, resultTypeKey, futureKey, error);
    if (poll->body == nullptr)
    {
        return nullptr;
    }
    // NS304-T07-F06: the poll captures the original callable's parameters into its frame (the
    // constructor stores them), so a `ref`/`in` parameter used after a `yield` reaches the source.
    poll->generatorParameterTypeKeys = function.parameterTypeKeys;

    // Constructor: `future<T> self = default; self.pollFn = F$poll; return self;` -- lazy, never
    // runs the body, but carries the poll function pointer so `await` over a variable works.
    auto constructorBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t selfOrdinal = nextSyntheticLocalOrdinal_++;
    auto selfDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, futureKey);
    selfDecl->localOrdinal = selfOrdinal;
    selfDecl->stringValue = "self";
    selfDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, futureKey));
    constructorBody->children.push_back(std::move(selfDecl));

    auto pollField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    pollField->memberOrdinal = 3; // pollFn
    pollField->elementTypeKey = futureKey;
    auto selfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, futureKey + "*");
    selfAddress->localOrdinal = selfOrdinal;
    pollField->children.push_back(std::move(selfAddress));
    auto pollAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    pollAddress->stringValue = function.mangledName + "$poll";
    auto pollStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    pollStore->operation = lexer::TokenKind::Equal;
    pollStore->children.push_back(std::move(pollField));
    pollStore->children.push_back(std::move(pollAddress));
    constructorBody->children.push_back(std::move(pollStore));

    auto retSelf = makeNode(ProjectOopHirNodeKind::Return, syntax, futureKey);
    auto selfReturn = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, futureKey);
    selfReturn->localOrdinal = selfOrdinal;
    retSelf->children.push_back(std::move(selfReturn));
    constructorBody->children.push_back(std::move(retSelf));
    function.body = std::move(constructorBody);
    // NS304-T07-F06: the original callable is now the async constructor — it allocates the frame
    // and stores the captured parameters (ref/in used after a yield), like the generator does.
    function.isGeneratorConstructor = true;
    return poll;
}

std::pair<std::unique_ptr<ProjectOopHirFunction>, std::unique_ptr<ProjectOopHirFunction>>
ProjectOopHirModel::splitGeneratorFunction(
    ProjectOopHirFunction& function,
    std::string& error)
{
    if (function.body == nullptr)
    {
        error = "generator has no lowered body to split";
        return {nullptr, nullptr};
    }
    const std::string iteratorKey = function.returnTypeKey;
    auto item = language::iteratorInnerTypeKey(iteratorKey);
    if (!item.has_value()) item = language::asyncIteratorInnerTypeKey(iteratorKey);
    if (!item.has_value())
    {
        error = "generator return type is not an iterator surface: " + iteratorKey;
        return {nullptr, nullptr};
    }
    const std::string itemTypeKey(*item);
    const ast::AstNode* syntax = function.body->syntax;

    // MoveNext: `bool Gen$moveNext(iterator<T>* self)` runs the body to the next yield. The body's
    // YieldValue/YieldBreak nodes are lowered by the LLVM (item + state + return true/false).
    auto moveNext = std::make_unique<ProjectOopHirFunction>();
    moveNext->memberOrdinal = function.memberOrdinal;
    moveNext->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    moveNext->declaringTypeOrdinal = function.declaringTypeOrdinal;
    moveNext->isStatic = true;
    moveNext->inliningPolicy = function.inliningPolicy;
    moveNext->mangledName = function.mangledName + "$moveNext";
    moveNext->debugDisplayName = function.debugDisplayName + "$moveNext";
    moveNext->returnTypeKey = "builtin:bool";
    moveNext->errorTypeKey = function.errorTypeKey;
    moveNext->effects = function.effects;
    moveNext->returnOwnership = sema::OwnershipKind::None;
    moveNext->parameterNames.push_back("self");
    moveNext->parameterTypeKeys.push_back(iteratorKey + "*");
    moveNext->parameterPassKinds.push_back(ParameterPassKind::Value);
    moveNext->parameterOwnerships.push_back(sema::OwnershipKind::None);
    moveNext->body = std::move(function.body);
    // NS304-T06-F04: the MoveNext captures the original callable's parameters into its frame.
    moveNext->generatorParameterTypeKeys = function.parameterTypeKeys;

    // NS304-T06-F07: `throw E;` becomes a typed generator error, not a native unwind:
    // { self->error = E; return false; } (the consumer checks state/error and rethrows).
    const std::optional<std::string_view> errorType = language::iteratorErrorTypeKey(iteratorKey);
    if (errorType.has_value())
    {
        rewriteGeneratorThrows(moveNext->body.get(), *errorType, iteratorKey);
    }

    // NS304-T06-F03: number the yields 1..N (the state set after each yield; 0 is the entry state)
    // and append the terminal `return false` after the last yield.
    std::size_t nextYieldState = 0;
    assignYieldStates(moveNext->body.get(), nextYieldState);
    auto wrapped = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    wrapped->children.push_back(std::move(moveNext->body));
    auto retFalse = makeNode(ProjectOopHirNodeKind::Return, syntax, "builtin:bool");
    auto falseValue = makeNode(ProjectOopHirNodeKind::BooleanConstant, syntax, "builtin:bool");
    falseValue->booleanValue = false;
    retFalse->children.push_back(std::move(falseValue));
    wrapped->children.push_back(std::move(retFalse));
    moveNext->body = std::move(wrapped);

    // Constructor: `iterator<T> self = default; self.moveNextFn = Gen$moveNext; return self;` --
    // lazy, never runs the body, but carries the MoveNext pointer so `it.MoveNext()` works.
    auto constructorBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t selfOrdinal = nextSyntheticLocalOrdinal_++;
    auto selfDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, iteratorKey);
    selfDecl->localOrdinal = selfOrdinal;
    selfDecl->stringValue = "self";
    selfDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, iteratorKey));
    constructorBody->children.push_back(std::move(selfDecl));

    auto moveNextField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    moveNextField->memberOrdinal = 3; // moveNextFn
    moveNextField->elementTypeKey = iteratorKey;
    auto selfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    selfAddress->localOrdinal = selfOrdinal;
    moveNextField->children.push_back(std::move(selfAddress));
    auto moveNextAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    moveNextAddress->stringValue = function.mangledName + "$moveNext";
    auto moveNextStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    moveNextStore->operation = lexer::TokenKind::Equal;
    moveNextStore->children.push_back(std::move(moveNextField));
    moveNextStore->children.push_back(std::move(moveNextAddress));
    constructorBody->children.push_back(std::move(moveNextStore));

    // NS304-T06-F07: also carry the Drop function pointer (field 4).
    auto dropField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    dropField->memberOrdinal = 4; // dropFn
    dropField->elementTypeKey = iteratorKey;
    auto dropSelfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    dropSelfAddress->localOrdinal = selfOrdinal;
    dropField->children.push_back(std::move(dropSelfAddress));
    auto dropAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    dropAddress->stringValue = function.mangledName + "$drop";
    auto dropStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    dropStore->operation = lexer::TokenKind::Equal;
    dropStore->children.push_back(std::move(dropField));
    dropStore->children.push_back(std::move(dropAddress));
    constructorBody->children.push_back(std::move(dropStore));

    auto retSelf = makeNode(ProjectOopHirNodeKind::Return, syntax, iteratorKey);
    auto selfReturn = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, iteratorKey);
    selfReturn->localOrdinal = selfOrdinal;
    retSelf->children.push_back(std::move(selfReturn));
    constructorBody->children.push_back(std::move(retSelf));
    function.body = std::move(constructorBody);
    // NS304-T06-F04: the original callable is now the generator constructor — it must allocate the
    // frame and store the captured parameters.
    function.isGeneratorConstructor = true;

    // Drop: `void Gen$drop(iterator<T>* self)` destroys the frame's live locals on early stop.
    // The drop-glue body is emitted by the LLVM from the MoveNext's frame locals.
    auto drop = std::make_unique<ProjectOopHirFunction>();
    drop->memberOrdinal = function.memberOrdinal;
    drop->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    drop->declaringTypeOrdinal = function.declaringTypeOrdinal;
    drop->isStatic = true;
    drop->inliningPolicy = function.inliningPolicy;
    drop->mangledName = function.mangledName + "$drop";
    drop->debugDisplayName = function.debugDisplayName + "$drop";
    drop->returnTypeKey = "builtin:void";
    drop->errorTypeKey = function.errorTypeKey;
    drop->effects = function.effects;
    drop->returnOwnership = sema::OwnershipKind::None;
    drop->parameterNames.push_back("self");
    drop->parameterTypeKeys.push_back(iteratorKey + "*");
    drop->parameterPassKinds.push_back(ParameterPassKind::Value);
    drop->parameterOwnerships.push_back(sema::OwnershipKind::None);
    auto dropBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    auto dropFrame = makeNode(ProjectOopHirNodeKind::DropFrame, syntax, "builtin:void");
    auto dropSelf = makeNode(ProjectOopHirNodeKind::ParameterValue, syntax, iteratorKey + "*");
    dropSelf->parameterOrdinal = 0;
    dropFrame->children.push_back(std::move(dropSelf));
    dropBody->children.push_back(std::move(dropFrame));
    drop->body = std::move(dropBody);

    return {std::move(moveNext), std::move(drop)};
}

std::vector<std::unique_ptr<ProjectOopHirFunction>> ProjectOopHirModel::splitAsyncGeneratorFunction(
    ProjectOopHirFunction& function,
    std::string& error)
{
    if (function.body == nullptr)
    {
        error = "async generator has no lowered body to split";
        return {};
    }
    const std::string iteratorKey = function.returnTypeKey;
    const auto item = language::asyncIteratorInnerTypeKey(iteratorKey);
    if (!item.has_value())
    {
        error = "async generator return type is not an async-iterator surface: " + iteratorKey;
        return {};
    }
    const std::string futureKey = language::futureTypeKey(
        "builtin:bool",
        language::asyncIteratorErrorTypeKey(iteratorKey).has_value()
            ? std::string(*language::asyncIteratorErrorTypeKey(iteratorKey))
            : std::string{});
    const std::string errorTypeKey =
        language::asyncIteratorErrorTypeKey(iteratorKey).has_value()
            ? std::string(*language::asyncIteratorErrorTypeKey(iteratorKey))
            : std::string{};
    const ast::AstNode* syntax = function.body->syntax;

    // NS304-T07-F02: number the yields 1..N (state after each yield) and append the terminal
    // `return false` (no more items), exactly like splitGeneratorFunction.
    std::size_t nextYieldState = 0;
    assignYieldStates(function.body.get(), nextYieldState);
    auto wrapped = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    wrapped->children.push_back(std::move(function.body));
    auto retFalse = makeNode(ProjectOopHirNodeKind::Return, syntax, "builtin:bool");
    auto falseValue = makeNode(ProjectOopHirNodeKind::BooleanConstant, syntax, "builtin:bool");
    falseValue->booleanValue = false;
    retFalse->children.push_back(std::move(falseValue));
    wrapped->children.push_back(std::move(retFalse));
    function.body = std::move(wrapped);

    // 1. The poll: `void Gen$moveNext$poll(future<bool>* self)` — runs the body to the next yield
    //    or await. rewriteAsyncPollReturns maps `return` -> result/state and `yield` -> AsyncYieldValue.
    auto poll = std::make_unique<ProjectOopHirFunction>();
    poll->memberOrdinal = function.memberOrdinal;
    poll->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    poll->declaringTypeOrdinal = function.declaringTypeOrdinal;
    poll->isStatic = true;
    poll->isAsyncPoll = true;
    poll->inliningPolicy = function.inliningPolicy;
    poll->mangledName = function.mangledName + "$moveNext$poll";
    poll->debugDisplayName = function.debugDisplayName + "$moveNext$poll";
    poll->returnTypeKey = "builtin:void";
    poll->errorTypeKey = errorTypeKey;
    poll->effects = function.effects;
    poll->returnOwnership = sema::OwnershipKind::None;
    poll->parameterNames.push_back("self");
    poll->parameterTypeKeys.push_back(futureKey + "*");
    poll->parameterPassKinds.push_back(ParameterPassKind::Value);
    poll->parameterOwnerships.push_back(sema::OwnershipKind::None);
    // NS304-T17-F09: the async-generator poll captures the original callable's parameters into the
    // frame (stored by the constructor), exactly like splitAsyncFunction does for an async method.
    // Without this the poll's asyncFrameParameters stays empty and a `ParameterValue` (e.g. `n` in
    // `if (n < 0) yield return -1`) falls back to the poll's `self` future pointer — a garbage read.
    poll->generatorParameterTypeKeys = function.parameterTypeKeys;
    poll->body = rewriteAsyncPollReturns(
        std::move(function.body), 0, "builtin:bool", futureKey, error);
    if (poll->body == nullptr) return {};

    // 2. The MoveNext: `future<bool> Gen$moveNext(async-iterator* self)` — creates the demand future,
    //    carries the async-iterator in future.frame, and points future.pollFn at the poll.
    auto moveNext = std::make_unique<ProjectOopHirFunction>();
    moveNext->memberOrdinal = function.memberOrdinal;
    moveNext->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    moveNext->declaringTypeOrdinal = function.declaringTypeOrdinal;
    moveNext->isStatic = true;
    moveNext->inliningPolicy = function.inliningPolicy;
    moveNext->mangledName = function.mangledName + "$moveNext";
    moveNext->debugDisplayName = function.debugDisplayName + "$moveNext";
    moveNext->returnTypeKey = futureKey;
    moveNext->errorTypeKey = errorTypeKey;
    moveNext->effects = function.effects;
    moveNext->returnOwnership = sema::OwnershipKind::None;
    moveNext->parameterNames.push_back("self");
    moveNext->parameterTypeKeys.push_back(iteratorKey + "*");
    moveNext->parameterPassKinds.push_back(ParameterPassKind::Value);
    moveNext->parameterOwnerships.push_back(sema::OwnershipKind::None);

    auto moveNextBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t futureOrdinal = nextSyntheticLocalOrdinal_++;
    auto futureDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, futureKey);
    futureDecl->localOrdinal = futureOrdinal;
    futureDecl->stringValue = "f";
    futureDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, futureKey));
    moveNextBody->children.push_back(std::move(futureDecl));
    // future.frame = self (the async-iterator).
    auto frameField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    frameField->memberOrdinal = 1;
    frameField->elementTypeKey = futureKey;
    auto futureAddr = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, futureKey + "*");
    futureAddr->localOrdinal = futureOrdinal;
    frameField->children.push_back(std::move(futureAddr));
    auto selfParam = makeNode(ProjectOopHirNodeKind::ParameterValue, syntax, iteratorKey + "*");
    selfParam->parameterOrdinal = 0;
    auto frameStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    frameStore->operation = lexer::TokenKind::Equal;
    frameStore->children.push_back(std::move(frameField));
    frameStore->children.push_back(std::move(selfParam));
    moveNextBody->children.push_back(std::move(frameStore));
    // future.pollFn = Gen$moveNext$poll.
    auto pollField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    pollField->memberOrdinal = 3;
    pollField->elementTypeKey = futureKey;
    auto futureAddr2 = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, futureKey + "*");
    futureAddr2->localOrdinal = futureOrdinal;
    pollField->children.push_back(std::move(futureAddr2));
    auto pollAddr = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    pollAddr->stringValue = function.mangledName + "$moveNext$poll";
    auto pollStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    pollStore->operation = lexer::TokenKind::Equal;
    pollStore->children.push_back(std::move(pollField));
    pollStore->children.push_back(std::move(pollAddr));
    moveNextBody->children.push_back(std::move(pollStore));
    // return future.
    auto retFuture = makeNode(ProjectOopHirNodeKind::Return, syntax, futureKey);
    auto futureValue = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, futureKey);
    futureValue->localOrdinal = futureOrdinal;
    retFuture->children.push_back(std::move(futureValue));
    moveNextBody->children.push_back(std::move(retFuture));
    moveNext->body = std::move(moveNextBody);

    // 3. The drop: `void Gen$drop(async-iterator* self)` — destroys the frame's live locals.
    auto drop = std::make_unique<ProjectOopHirFunction>();
    drop->memberOrdinal = function.memberOrdinal;
    drop->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    drop->declaringTypeOrdinal = function.declaringTypeOrdinal;
    drop->isStatic = true;
    drop->inliningPolicy = function.inliningPolicy;
    drop->mangledName = function.mangledName + "$drop";
    drop->debugDisplayName = function.debugDisplayName + "$drop";
    drop->returnTypeKey = "builtin:void";
    drop->errorTypeKey = function.errorTypeKey;
    drop->effects = function.effects;
    drop->returnOwnership = sema::OwnershipKind::None;
    drop->parameterNames.push_back("self");
    drop->parameterTypeKeys.push_back(iteratorKey + "*");
    drop->parameterPassKinds.push_back(ParameterPassKind::Value);
    drop->parameterOwnerships.push_back(sema::OwnershipKind::None);
    auto dropBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    auto dropFrame = makeNode(ProjectOopHirNodeKind::DropFrame, syntax, "builtin:void");
    auto dropSelf = makeNode(ProjectOopHirNodeKind::ParameterValue, syntax, iteratorKey + "*");
    dropSelf->parameterOrdinal = 0;
    dropFrame->children.push_back(std::move(dropSelf));
    dropBody->children.push_back(std::move(dropFrame));
    drop->body = std::move(dropBody);

    // 4. The constructor (the original function): async-iterator + moveNextFn + dropFn.
    auto constructorBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t selfOrdinal = nextSyntheticLocalOrdinal_++;
    auto selfDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, iteratorKey);
    selfDecl->localOrdinal = selfOrdinal;
    selfDecl->stringValue = "self";
    selfDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, iteratorKey));
    constructorBody->children.push_back(std::move(selfDecl));
    auto moveNextField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    moveNextField->memberOrdinal = 3; // moveNextFn
    moveNextField->elementTypeKey = iteratorKey;
    auto selfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    selfAddress->localOrdinal = selfOrdinal;
    moveNextField->children.push_back(std::move(selfAddress));
    auto moveNextAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    moveNextAddress->stringValue = function.mangledName + "$moveNext";
    auto moveNextStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    moveNextStore->operation = lexer::TokenKind::Equal;
    moveNextStore->children.push_back(std::move(moveNextField));
    moveNextStore->children.push_back(std::move(moveNextAddress));
    constructorBody->children.push_back(std::move(moveNextStore));
    auto dropField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    dropField->memberOrdinal = 4; // dropFn
    dropField->elementTypeKey = iteratorKey;
    auto dropSelfAddr = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    dropSelfAddr->localOrdinal = selfOrdinal;
    dropField->children.push_back(std::move(dropSelfAddr));
    auto dropAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    dropAddress->stringValue = function.mangledName + "$drop";
    auto dropStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    dropStore->operation = lexer::TokenKind::Equal;
    dropStore->children.push_back(std::move(dropField));
    dropStore->children.push_back(std::move(dropAddress));
    constructorBody->children.push_back(std::move(dropStore));
    auto retSelf = makeNode(ProjectOopHirNodeKind::Return, syntax, iteratorKey);
    auto selfReturn = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, iteratorKey);
    selfReturn->localOrdinal = selfOrdinal;
    retSelf->children.push_back(std::move(selfReturn));
    constructorBody->children.push_back(std::move(retSelf));
    function.body = std::move(constructorBody);
    // NS304-T07-F04: the original callable is the async-generator constructor — it must allocate the
    // frame (heap) so `ref` items reference persistent slots, not the poll's stack.
    function.isGeneratorConstructor = true;

    std::vector<std::unique_ptr<ProjectOopHirFunction>> result;
    result.push_back(std::move(moveNext));
    result.push_back(std::move(poll));
    result.push_back(std::move(drop));
    return result;
}

std::vector<std::size_t> ProjectOopHirModel::collectAwaitedMemberOrdinals(
    const ast::AstNode* root) const
{
    std::vector<std::size_t> result;
    std::vector<const ast::AstNode*> pending;
    pending.push_back(root);
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr) continue;
        if (node->kind() == ast::AstNodeKind::AwaitExpression)
        {
            const MethodCallBinding* awaitedBinding = callBinding(node->first());
            if (awaitedBinding != nullptr && awaitedBinding->selectedMemberOrdinal.has_value())
                result.push_back(*awaitedBinding->selectedMemberOrdinal);
            continue;
        }
        const auto children = node->children();
        for (auto child = children.rbegin(); child != children.rend(); ++child)
            pending.push_back(*child);
        pending.push_back(node->second());
        pending.push_back(node->first());
    }
    return result;
}


bool ProjectOopHirModel::awaitProducerCanSuspend(std::size_t rootMemberOrdinal) const
{
    std::unordered_set<std::size_t> visitedSuspendMembers;
    std::vector<std::size_t> pendingMembers = {rootMemberOrdinal};
    while (!pendingMembers.empty())
    {
        const std::size_t memberIdx = pendingMembers.back();
        pendingMembers.pop_back();
        if (!visitedSuspendMembers.insert(memberIdx).second) continue;
        if (memberIdx >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& awaitedMember = memberModel_->members()[memberIdx];
        if (awaitedMember.syntax == nullptr) continue;
        const auto awaitedOrdinals = collectAwaitedMemberOrdinals(awaitedMember.syntax->first());
        for (auto awaited = awaitedOrdinals.rbegin(); awaited != awaitedOrdinals.rend(); ++awaited)
            if (*awaited < memberModel_->members().size()) pendingMembers.push_back(*awaited);
    }
    // Preserve the effective Stage 34 policy: the scan is cycle-safe but currently never rejects.
    return false;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerAwaitExpression(
    const ast::AstNode* expression,
    std::unique_ptr<ProjectOopHirNode> operand,
    std::optional<std::size_t> selectedMemberOrdinal,
    std::vector<std::size_t> cleanupRegionOrdinals,
    std::optional<std::size_t> exceptionTargetRegionOrdinal,
    std::string& error)
{
    const auto inner = language::futureInnerTypeKey(operand->typeKey);
    if (!inner.has_value())
    {
        error = selectedMemberOrdinal.has_value()
            ? "await operand does not produce a future surface: " + operand->typeKey
            : "await operand is neither an async call nor a future value: " + operand->typeKey;
        return nullptr;
    }

    auto await = makeNode(ProjectOopHirNodeKind::AwaitFuture, expression, std::string(*inner));
    await->elementTypeKey = operand->typeKey;
    await->integerValue = loweringAsyncBody_ ? 1 : 0;
    if (selectedMemberOrdinal.has_value())
    {
        const ClassMemberSymbol& callee = memberModel_->members()[*selectedMemberOrdinal];
        await->stringValue = mangleCallable(
            nameLookup_->namedTypes()[callee.declaringTypeOrdinal], callee) + "$poll";
        await->cleanupRegionOrdinals = std::move(cleanupRegionOrdinals);
        await->exceptionTargetRegionOrdinal = exceptionTargetRegionOrdinal;
    }
    else await->stringValue.clear();
    await->children.push_back(std::move(operand));
    return await;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerCoroutineStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    const auto attachStructuredExit = [&](ProjectOopHirNode& node)
    {
        applyStructuredExitBinding(node, statement);
    };

    switch (statement->kind())
    {
        case ast::AstNodeKind::SuspendStatement:
        {
            // NS304-T05-F09: a bare `yield;` suspends the poll once. integerValue is the resume
            // state index (1 for the single-suspension case; multi-suspension generalizes like
            // the await resume indices).
            auto node = makeNode(ProjectOopHirNodeKind::SuspendPoint, statement, "builtin:void");
            node->integerValue = 1;
            return node;
        }

        case ast::AstNodeKind::YieldReturnStatement:
        {
            if (statement->first() != nullptr)
                return lowerCoroutineValueStatement(statement, memberOrdinal, error);
            // NS304-T06-F03: `yield return v` becomes a YieldValue node. integerValue is the yield
            // state index, assigned by splitGeneratorFunction (the MoveNext rewrite).
            auto node = makeNode(ProjectOopHirNodeKind::YieldValue, statement, "builtin:void");
            node->integerValue = 0;
            // NS304-T07-F05: a `yield return` inside a try/finally suspends while the finally is
            // still active; capture those regions directly so a cooperative cancellation runs them
            // once. (Not a structured exit, so the structuredExitBinding path does not apply.)
            node->cleanupRegionOrdinals.assign(
                activeHirCleanupRegions_.begin(), activeHirCleanupRegions_.end());
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            return node;
        }

        case ast::AstNodeKind::YieldBreakStatement:
        {
            // NS304-T06-F07: `yield break` terminates the generator and must run the active cleanup
            // regions (defer/drop) exactly once before returning false.
            auto node = makeNode(ProjectOopHirNodeKind::YieldBreak, statement, "builtin:void");
            attachStructuredExit(*node);
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            return node;
        }
        default:
            error = "non-coroutine statement reached HIR coroutine lowering";
            return nullptr;
    }
}
}
