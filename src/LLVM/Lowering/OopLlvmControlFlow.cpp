#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::collectTryHandlers(
        const frontend::ProjectOopMirNode* node,
        std::vector<const frontend::ProjectOopMirNode*>& catchNodes,
        const frontend::ProjectOopMirNode*& finallyNode,
        std::string& error)
{
        bool sawCatchAll = false;
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* child = node->children[index].get();
            if (child == nullptr) continue;
            if (child->kind == frontend::ProjectOopMirNodeKind::FinallyRegion)
            {
                if (finallyNode != nullptr)
                    return fail(error, "Stage 45.11 try MIR has duplicate finally regions");
                finallyNode = child;
                continue;
            }
            if (child->kind != frontend::ProjectOopMirNodeKind::CatchRegion)
                return fail(error, "Stage 45.11 try MIR contains an unexpected child region");
            if (sawCatchAll)
                return fail(error, "Stage 45.11 catch-all handler must remain terminal");
            const std::size_t expectedChildren = child->catchHasFilter ? 2U : 1U;
            if (child->children.size() != expectedChildren ||
                std::ranges::any_of(child->children, [](const auto& value)
                {
                    return value == nullptr;
                }))
                return fail(error, "Stage 45.11 catch MIR handler body is incomplete");
            if (child->catchAll)
            {
                if (child->exceptionTypeOrdinal.has_value())
                    return fail(error, "Stage 45.11 catch-all unexpectedly carries a typed RTTI ordinal");
                if (!child->catchHasFilter) sawCatchAll = true;
            }
            else
            {
                if (!child->exceptionTypeOrdinal.has_value())
                    return fail(error, "Stage 45.11 typed catch has no exception type ordinal");
                if (!typedCatchFilters_.contains(*child->exceptionTypeOrdinal))
                    return fail(error, "Stage 45.11 typed catch cannot resolve its RTTI filter");
            }
            catchNodes.push_back(child);
        }
        return true;
    }

void OopLlvmImplementation::createTryHandlerBlocks(
        std::size_t catchCount,
        bool hasFinally,
        LLVMBasicBlockRef& continueBlock,
        LLVMBasicBlockRef& normalFinallyBlock,
        LLVMBasicBlockRef& dispatchBlock,
        std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        std::vector<LLVMBasicBlockRef>& catchBlocks,
        std::vector<LLVMBasicBlockRef>& handlerBlocks,
        std::vector<LLVMBasicBlockRef>& handledBlocks)
{
        continueBlock = appendBlock("eh.try.cont");
        normalFinallyBlock = hasFinally ? appendBlock("eh.finally.normal") : nullptr;
        dispatchBlock = catchCount != 0U ? appendBlock("eh.catch.dispatch") : nullptr;
        clauseTestBlocks.reserve(catchCount);
        catchBlocks.reserve(catchCount);
        handlerBlocks.reserve(catchCount);
        handledBlocks.reserve(catchCount);
        for (std::size_t index = 0; index < catchCount; ++index)
        {
            clauseTestBlocks.push_back(appendBlock("eh.catch.test." + std::to_string(index)));
            catchBlocks.push_back(appendBlock("eh.catch." + std::to_string(index)));
            handlerBlocks.push_back(appendBlock("eh.catch.body." + std::to_string(index)));
            handledBlocks.push_back(appendBlock("eh.catch.handled." + std::to_string(index)));
        }
    }

bool OopLlvmImplementation::lowerTryProtectedBody(
        const frontend::ProjectOopMirNode* node,
        bool hasFinally,
        LLVMBasicBlockRef normalFinallyBlock,
        LLVMBasicBlockRef continueBlock,
        LLVMBasicBlockRef dispatchBlock,
        bool hasCatches,
        std::string& error)
{
        if (hasCatches)
        {
            exceptionDispatchBlocks_[*node->mirRegionOrdinal] = dispatchBlock;
            exceptionHandlerStack_.push_back(ExceptionHandlerContext{
                *node->mirRegionOrdinal, dispatchBlock,
                activeCleanupRegions_.size(), funcletStack_.size()});
        }
        const bool protectedOk = lowerStatement(node->children.front().get(), error);
        if (hasCatches)
        {
            exceptionHandlerStack_.pop_back();
            exceptionDispatchBlocks_.erase(*node->mirRegionOrdinal);
        }
        if (!protectedOk)
        {
            if (hasFinally) activeCleanupRegions_.pop_back();
            return false;
        }
        if (!terminated()) LLVMBuildBr(
            builder_, hasFinally ? normalFinallyBlock : continueBlock);
        return true;
    }

bool OopLlvmImplementation::beginTryCatchDispatch(
        LLVMBasicBlockRef dispatchBlock,
        bool hasFinally,
        LLVMValueRef& caughtException,
        LLVMBasicBlockRef& unmatchedBlock,
        std::string& error)
{
        LLVMPositionBuilderAtEnd(builder_, dispatchBlock);
        LLVMValueRef catchSwitch = LLVMBuildCatchSwitch(
            builder_, activeFuncletPad(), nullptr, 1, "eh.catchswitch");
        if (catchSwitch == nullptr)
        {
            if (hasFinally) activeCleanupRegions_.pop_back();
            return fail(error, "Stage 45.11 failed to create catchswitch");
        }
        LLVMBasicBlockRef padBlock = appendBlock("eh.catchpad.entry");
        LLVMAddHandler(catchSwitch, padBlock);
        LLVMBasicBlockRef selectBlock = appendBlock("eh.catch.select");
        LLVMPositionBuilderAtEnd(builder_, padBlock);
        LLVMValueRef catchArguments[] = {ehFilterFunction_};
        LLVMValueRef catchPad = LLVMBuildCatchPad(
            builder_, catchSwitch, catchArguments, 1, "eh.catchpad");
        if (catchPad == nullptr)
        {
            if (hasFinally) activeCleanupRegions_.pop_back();
            return fail(error, "Stage 45.11 failed to create catchpad");
        }
        LLVMBuildCatchRet(builder_, catchPad, selectBlock);
        LLVMPositionBuilderAtEnd(builder_, selectBlock);
        caughtException = buildNoUnwindCall(
            exceptionCurrentFunctionType_, exceptionCurrentFunction_, nullptr, 0,
            "eh.caught.exception");
        if (caughtException == nullptr)
        {
            if (hasFinally) activeCleanupRegions_.pop_back();
            return fail(error, "Stage 45.11 could not read the caught exception object");
        }
        unmatchedBlock = appendBlock("eh.catch.unmatched");
        return true;
    }

bool OopLlvmImplementation::emitTryCatchClauseTests(
        const std::vector<const frontend::ProjectOopMirNode*>& catchNodes,
        const std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        const std::vector<LLVMBasicBlockRef>& catchBlocks,
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        bool hasFinally,
        std::string& error)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMBuildBr(builder_, clauseTestBlocks.front());
        for (std::size_t index = 0; index < catchNodes.size(); ++index)
        {
            const frontend::ProjectOopMirNode* catchNode = catchNodes[index];
            LLVMPositionBuilderAtEnd(builder_, clauseTestBlocks[index]);
            LLVMBasicBlockRef nextTest = index + 1U < catchNodes.size()
                ? clauseTestBlocks[index + 1U] : unmatchedBlock;
            if (catchNode->catchAll)
            {
                LLVMBuildBr(builder_, catchBlocks[index]);
                continue;
            }
            const auto typeIdFound = typedCatchTypeIds_.find(*catchNode->exceptionTypeOrdinal);
            if (typeIdFound == typedCatchTypeIds_.end())
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return fail(error, "Stage 45.11 lost a typed catch RTTI type id during lowering");
            }
            LLVMValueRef arguments[] = {
                caughtException, LLVMConstInt(i64, typeIdFound->second, 0)};
            LLVMValueRef matches = buildNoUnwindCall(
                rttiTestFunctionType_, rttiClassTestFunction_, arguments, 2, "eh.catch.matches");
            if (matches == nullptr)
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return fail(error, "Stage 45.11 failed to emit a catch clause RTTI test");
            }
            LLVMBuildCondBr(builder_, matches, catchBlocks[index], nextTest);
        }
        return true;
    }

bool OopLlvmImplementation::emitUnmatchedTryCatch(
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        bool hasFinally,
        std::string& error)
{
        LLVMPositionBuilderAtEnd(builder_, unmatchedBlock);
        const std::vector<std::size_t> unmatchedCleanupPath = activeCleanupRegions_;
        for (auto ordinal = unmatchedCleanupPath.rbegin();
             ordinal != unmatchedCleanupPath.rend(); ++ordinal)
        {
            if (emitCleanupAction(*ordinal, error)) continue;
            if (hasFinally) activeCleanupRegions_.pop_back();
            return false;
        }
        LLVMValueRef arguments[] = {caughtException};
        buildNoUnwindCall(nativeThrowFunctionType_, nativeThrowFunction_, arguments, 1, "");
        LLVMBuildUnreachable(builder_);
        return true;
    }

bool OopLlvmImplementation::emitTryCatchBodies(
        const std::vector<const frontend::ProjectOopMirNode*>& catchNodes,
        const std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        const std::vector<LLVMBasicBlockRef>& catchBlocks,
        const std::vector<LLVMBasicBlockRef>& handlerBlocks,
        const std::vector<LLVMBasicBlockRef>& handledBlocks,
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        LLVMBasicBlockRef normalFinallyBlock,
        LLVMBasicBlockRef continueBlock,
        bool hasFinally,
        std::string& error)
{
        for (std::size_t index = 0; index < catchNodes.size(); ++index)
        {
            if (!emitOneTryCatchBody(
                    catchNodes[index], index, clauseTestBlocks, catchBlocks[index],
                    handlerBlocks[index], handledBlocks[index], caughtException,
                    unmatchedBlock, normalFinallyBlock, continueBlock, hasFinally, error))
                return false;
        }
        return true;
    }

bool OopLlvmImplementation::emitOneTryCatchBody(
        const frontend::ProjectOopMirNode* catchNode,
        std::size_t index,
        const std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        LLVMBasicBlockRef catchBlock,
        LLVMBasicBlockRef handlerBlock,
        LLVMBasicBlockRef handledBlock,
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        LLVMBasicBlockRef normalFinallyBlock,
        LLVMBasicBlockRef continueBlock,
        bool hasFinally,
        std::string& error)
{
        LLVMPositionBuilderAtEnd(builder_, catchBlock);
        LLVMMetadataRef catchScope = createDebugLexicalScope(catchNode, currentDebugScope_);
        ScopedDebugScope scopedCatchDebugScope(currentDebugScope_, catchScope);
        if (catchNode->localOrdinal.has_value())
        {
            const auto localFound = localStorage_.find(*catchNode->localOrdinal);
            if (localFound == localStorage_.end() || localFound->second == nullptr)
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return fail(error, "Stage 45.11 typed catch local has no LLVM storage");
            }
            LLVMBuildStore(builder_, caughtException, localFound->second);
            if (!emitDebugCatchParameter(catchNode, localFound->second, index, error))
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return false;
            }
        }
        if (catchNode->catchHasFilter)
        {
            LLVMValueRef filter = lowerExpression(catchNode->children.front().get(), error);
            if (filter == nullptr)
            {
                if (hasFinally) activeCleanupRegions_.pop_back();
                return false;
            }
            LLVMBasicBlockRef nextTest = index + 1U < clauseTestBlocks.size()
                ? clauseTestBlocks[index + 1U] : unmatchedBlock;
            LLVMBuildCondBr(builder_, filter, handlerBlock, nextTest);
        }
        else LLVMBuildBr(builder_, handlerBlock);
        LLVMPositionBuilderAtEnd(builder_, handlerBlock);
        // `rethrow` inside this body used to find its exception by walking funcletStack_ for
        // an active catch funclet. The body is ordinary parent code now, so it looks here.
        // CC1 EH-001: loopStack_.size() is recorded HERE, before the body is lowered, so
        // a `break`/`continue` inside it can tell "leaves this catch" from "leaves a loop
        // or switch opened inside this catch".
        activeCatchExceptions_.push_back(ActiveCatchBody{caughtException, loopStack_.size()});
        const std::size_t bodyIndex = catchNode->catchHasFilter ? 1U : 0U;
        const bool handlerOk = lowerStatement(catchNode->children[bodyIndex].get(), error);
        const bool handlerTerminated = terminated();
        activeCatchExceptions_.pop_back();
        if (!handlerOk)
        {
            if (hasFinally) activeCleanupRegions_.pop_back();
            return false;
        }
        if (!handlerTerminated) LLVMBuildBr(builder_, handledBlock);
        LLVMPositionBuilderAtEnd(builder_, handledBlock);
        // The O0 SEH legalization pass has to run mem2reg on this whole function (see
        // LlvmOptimizer.cpp).  A catch-local assignment can consequently disappear into
        // a PHI and leave the machine block with only this mandatory exception-clear
        // call.  Without an explicit source location on that surviving instruction,
        // CodeView inherited the enclosing `try` location and the catch body had no
        // stoppable line at all.  Attribute the handler-completion block to its first
        // source statement (or to `catch` for an empty body).  Instructions that survive
        // from the body keep their more precise nested locations; this is only the stable
        // anchor that mem2reg cannot erase.
        const frontend::ProjectOopMirNode* catchBody = catchNode->children[bodyIndex].get();
        const frontend::ProjectOopMirNode* catchAnchor = catchNode;
        if (catchBody != nullptr && catchBody->kind == frontend::ProjectOopMirNodeKind::Block &&
            !catchBody->children.empty() && catchBody->children.front() != nullptr)
            catchAnchor = catchBody->children.front().get();
        ScopedDebugLocation catchCompletionLocation(builder_, debugLocation(catchAnchor));
        buildNoUnwindCall(exceptionClearFunctionType_, exceptionClearFunction_, nullptr, 0, "");
        LLVMBuildBr(builder_, hasFinally ? normalFinallyBlock : continueBlock);
        return true;
    }

bool OopLlvmImplementation::lowerTryRegion(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || node->children.empty() || node->children.front() == nullptr)
            return fail(error, "Stage 45.11 try MIR region is incomplete");

        std::vector<const frontend::ProjectOopMirNode*> catchNodes;
        const frontend::ProjectOopMirNode* finallyNode = nullptr;
        if (!collectTryHandlers(node, catchNodes, finallyNode, error)) return false;

        const bool hasFinally = node->cleanupRegionOrdinal.has_value();
        if (hasFinally != (finallyNode != nullptr))
            return fail(error, "Stage 45.11 try/finally MIR cleanup identity is inconsistent");
        if (catchNodes.empty() && finallyNode == nullptr)
            return fail(error, "Stage 45.11 try region has neither catch nor finally");
        if (!catchNodes.empty() && !node->mirRegionOrdinal.has_value())
            return fail(error, "Stage 45.11 catch-bearing try has no MIR region identity");

        LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);

        LLVMBasicBlockRef continueBlock = nullptr;
        LLVMBasicBlockRef normalFinallyBlock = nullptr;
        LLVMBasicBlockRef dispatchBlock = nullptr;
        std::vector<LLVMBasicBlockRef> clauseTestBlocks;
        std::vector<LLVMBasicBlockRef> catchBlocks;
        std::vector<LLVMBasicBlockRef> handlerBlocks;
        std::vector<LLVMBasicBlockRef> handledBlocks;
        createTryHandlerBlocks(
            catchNodes.size(), hasFinally, continueBlock, normalFinallyBlock, dispatchBlock,
            clauseTestBlocks, catchBlocks, handlerBlocks, handledBlocks);

        if (hasFinally)
            activeCleanupRegions_.push_back(*node->cleanupRegionOrdinal);

        if (!lowerTryProtectedBody(
                node, hasFinally, normalFinallyBlock, continueBlock, dispatchBlock,
                !catchNodes.empty(), error)) return false;

        if (!catchNodes.empty())
        {
            LLVMPositionBuilderAtEnd(builder_, dispatchBlock);
            // NOTE: no buildCurrentUnwindDestination() call here any more. The catchswitch below
            // deliberately unwinds to caller, and the enclosing cleanups are run explicitly on the
            // "no clause matched" path instead -- see the long comment on the catchswitch for why.
            // Building the chain here as well would only emit unreachable cleanuppad blocks.
            // Bugs B-006/B-007 (silent premature destruction -- and therefore use-after-free -- of
            // any cleanup-owning local whose scope ENCLOSES a try whose catch actually catches; see
            // BUGS_ENCONTRADOS/bug_eh_raii_doble_destruccion_tras_catch_o0.md) AND the multi-catch
            // bug found while fixing them (a second `catch` clause never matched at all) share one
            // root and are fixed together here. Three facts drive this shape:
            //
            // 1. `__C_specific_handler` -- the Windows x64 personality this compiler targets -- runs
            //    an ENCLOSING cleanup while unwinding to a catch in the SAME frame. LLVM lowers a
            //    catchswitch's unwind edge to a __finally scope-table entry covering the TRY's
            //    instruction range, and the personality runs every such entry whose range contains
            //    the throw IP but not the unwind target IP; the catch handler always lands outside
            //    the try's range, so an enclosing cleanup is indistinguishable from one genuinely
            //    being exited. It therefore destroyed and freed objects that were still in scope.
            //    This is the personality's documented IP-range behaviour, NOT bad IR from this
            //    compiler: compiling the analogous C construct
            //    (`__try { __try {raise} __except {...} } __finally {cleanup}`) with clang and
            //    running it under this same Wine double-runs clang's own __finally identically. So
            //    the fix cannot be "shape the scope table better" -- it has to stop depending on
            //    that edge, which means the catchswitch unwinds TO CALLER.
            // 2. SEH honours exactly ONE handler per catchswitch. LLVM's SEH state numbering emits
            //    a scope-table entry for the first handler only -- verified on the emitted table.
            //    Everything past the first `catch` clause was therefore dead code, and a program
            //    whose matching clause was not the first one simply let the exception escape. That
            //    was a pre-existing P0 bug, independent of the two above, and it also rules out
            //    "just add a synthetic catch-all clause" as a way to solve point 1.
            // 3. A catch clause body does not need to live inside the funclet at all.
            //
            // Hence: ONE catchpad, whose filter is the generic "is this one of ours" filter, that
            // does nothing but `catchret` straight out to ordinary parent code. That parent code
            // re-tests the caught object against each clause's RTTI type IN SOURCE ORDER and jumps
            // to the matching clause body -- so every clause works, not just the first -- and when
            // no clause matches it runs the enclosing cleanups explicitly and re-raises. Because
            // the clause bodies are now ordinary parent blocks rather than funclet interiors, an
            // exception thrown inside a catch body is lowered by the normal machinery and unwinds
            // through the enclosing cleanups exactly like any other code in this scope, with none
            // of the funclet-parentage constraints that previously forced compromises here.
            // __nativesharp_eh_filter accepts any exception carrying this compiler's own exception
            // code -- exactly the set a NativeSharp `catch` could ever handle. A foreign SEH
            // exception (an access violation, say) is still declined here and propagates untouched.
            LLVMValueRef caughtException = nullptr;
            LLVMBasicBlockRef unmatchedBlock = nullptr;
            if (!beginTryCatchDispatch(
                    dispatchBlock, hasFinally, caughtException, unmatchedBlock, error)) return false;

            // Clause selection, in source order. A catch-all clause terminates the chain; anything
            // after an unfiltered catch-all was already rejected up front. A filtered catch-all can
            // decline the exception and therefore legitimately precedes another clause.
            if (!emitTryCatchClauseTests(
                    catchNodes, clauseTestBlocks, catchBlocks, caughtException,
                    unmatchedBlock, hasFinally, error)) return false;

            // No clause matched: this exception really is leaving the try, so the enclosing cleanup
            // regions must run -- innermost first, the order a real unwind would have used. They are
            // emitted as the same ordinary, guarded destroy sequences the normal scope-exit path
            // uses, so the exactly-once guard in emitRaiiDestroy/emitOwnedParameterDestroy covers
            // this path too. activeCleanupRegions_ still holds this try's own `finally` region here
            // (it is popped further below), which is correct: a `finally` must run before an
            // unmatched exception leaves its try.
            // Snapshot the path before lowering any cleanup. A finally body can contain another
            // cleanup region, and lowering that nested region grows activeCleanupRegions_. Walking
            // the live vector with reverse iterators would then retain an iterator across a
            // possible reallocation (debug MSVC diagnoses this as "can't decrement invalidated
            // vector iterator"; release builds have undefined behaviour).
            // Re-raise the very same exception object, so outer frames' typed filters still see the
            // type they would have seen had this frame never intervened. A plain call, not an
            // invoke: every cleanup this frame owns has just run above, so there is nothing left
            // for an unwind edge here to do.
            if (!emitUnmatchedTryCatch(
                    caughtException, unmatchedBlock, hasFinally, error) ||
                !emitTryCatchBodies(
                    catchNodes, clauseTestBlocks, catchBlocks, handlerBlocks, handledBlocks,
                    caughtException, unmatchedBlock, normalFinallyBlock, continueBlock,
                    hasFinally, error)) return false;
        }

        if (hasFinally)
        {
            activeCleanupRegions_.pop_back();
            LLVMPositionBuilderAtEnd(builder_, normalFinallyBlock);
            if (!emitCleanupAction(*node->cleanupRegionOrdinal, error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, continueBlock);
        }

        LLVMPositionBuilderAtEnd(builder_, continueBlock);
        return true;
    }

bool OopLlvmImplementation::lowerThrow(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 1 || node->children.front() == nullptr)
            return fail(error, "Stage 45.10 throw MIR requires one exception object value");
        LLVMValueRef exception = lowerExpression(node->children.front().get(), error);
        if (exception == nullptr) return false;
        if (terminated()) return true;
        return lowerExceptionalTransfer(exception, node, error);
    }

bool OopLlvmImplementation::lowerIf(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() < 2) return fail(error, "Stage 34 if MIR is incomplete");
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return false;
        if (terminated()) return true;
        LLVMBasicBlockRef thenBlock = appendBlock("if.then");
        LLVMBasicBlockRef elseBlock = node->children.size() >= 3 ? appendBlock("if.else") : nullptr;
        LLVMBasicBlockRef endBlock = appendBlock("if.end");
        LLVMBuildCondBr(builder_, condition, thenBlock, elseBlock != nullptr ? elseBlock : endBlock);

        LLVMPositionBuilderAtEnd(builder_, thenBlock);
        if (!lowerStatement(node->children[1].get(), error)) return false;
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        if (elseBlock != nullptr)
        {
            LLVMPositionBuilderAtEnd(builder_, elseBlock);
            if (!lowerStatement(node->children[2].get(), error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, endBlock);
        }
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

bool OopLlvmImplementation::lowerWhile(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 2) return fail(error, "Stage 34 while MIR is incomplete");
        LLVMBasicBlockRef condBlock = appendBlock("while.cond");
        LLVMBasicBlockRef bodyBlock = appendBlock("while.body");
        LLVMBasicBlockRef endBlock = appendBlock("while.end");
        LLVMBuildBr(builder_, condBlock);
        LLVMPositionBuilderAtEnd(builder_, condBlock);
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return false;
        if (terminated())
        {
            LLVMPositionBuilderAtEnd(builder_, endBlock);
            return true;
        }
        LLVMBuildCondBr(builder_, condition, bodyBlock, endBlock);
        loopStack_.push_back({
            endBlock, condBlock, funcletStack_.size(), funcletStack_.size(), {}, nullptr});
        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        const bool ok = lowerStatement(node->children[1].get(), error);
        if (ok && !terminated()) LLVMBuildBr(builder_, condBlock);
        loopStack_.pop_back();
        if (!ok) return false;
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerLoopExpression(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 1 || node->children.front() == nullptr ||
            node->children.front()->kind != frontend::ProjectOopMirNodeKind::Block)
        {
            return failValue(error, "T07-F05 loop expression MIR requires one block body");
        }

        LLVMValueRef resultStorage = nullptr;
        LLVMTypeRef resultType = nullptr;
        if (node->typeKey != "builtin:never" && node->typeKey != "builtin:void")
        {
            resultType = llvmType(node->typeKey, error);
            if (resultType == nullptr) return nullptr;
            resultStorage = LLVMBuildAlloca(builder_, resultType, "loop.result.addr");
        }

        LLVMBasicBlockRef bodyBlock = appendBlock("loop.body");
        LLVMBasicBlockRef endBlock = appendBlock("loop.end");
        LLVMBuildBr(builder_, bodyBlock);
        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        loopStack_.push_back({
            endBlock,
            bodyBlock,
            funcletStack_.size(),
            funcletStack_.size(),
            node->stringValue,
            resultStorage});
        const bool ok = lowerStatement(node->children.front().get(), error);
        if (ok && !terminated()) LLVMBuildBr(builder_, bodyBlock);
        loopStack_.pop_back();
        if (!ok) return nullptr;

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (node->typeKey == "builtin:never")
        {
            LLVMBuildUnreachable(builder_);
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }
        if (node->typeKey == "builtin:void")
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        return LLVMBuildLoad2(builder_, resultType, resultStorage, "loop.result");
    }

bool OopLlvmImplementation::lowerDoWhile(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 2) return fail(error, "do-while MIR is incomplete");
        LLVMBasicBlockRef bodyBlock = appendBlock("dowhile.body");
        LLVMBasicBlockRef condBlock = appendBlock("dowhile.cond");
        LLVMBasicBlockRef endBlock = appendBlock("dowhile.end");
        LLVMBuildBr(builder_, bodyBlock);
        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        loopStack_.push_back({
            endBlock, condBlock, funcletStack_.size(), funcletStack_.size(), {}, nullptr});
        const bool ok = lowerStatement(node->children[1].get(), error);
        if (ok && !terminated()) LLVMBuildBr(builder_, condBlock);
        loopStack_.pop_back();
        if (!ok) return false;
        LLVMPositionBuilderAtEnd(builder_, condBlock);
        LLVMValueRef condition = lowerExpression(node->children[0].get(), error);
        if (condition == nullptr) return false;
        if (!terminated()) LLVMBuildCondBr(builder_, condition, bodyBlock, endBlock);
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

bool OopLlvmImplementation::lowerSwitch(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.empty()) return fail(error, "switch MIR is incomplete");

        LLVMValueRef governing = lowerExpression(node->children[0].get(), error);
        if (governing == nullptr) return false;
        if (terminated()) return true;
        const std::string& governingTypeKey = node->children[0]->typeKey;
        LLVMTypeRef governingType = llvmType(governingTypeKey, error);
        if (governingType == nullptr) return false;
        // The frontend already rejected anything that is not integral or an enum, and llvmType maps
        // exactly those two to an integer carrier. Restating the rule here -- in type-key terms
        // rather than by probing the LLVM type -- keeps the backend honest if that ever changes,
        // and needs no new entry in LlvmCApi.h.
        if (!isEnumTypeKey(nameLookup_, governingTypeKey) &&
            !frontend::ProjectTypeConversion::isIntegral(governingTypeKey))
        {
            return fail(error, "switch governing value is not integral or an enum: " + governingTypeKey);
        }
        // EL SIGNO DE LAS CONSTANTES DE `case` es el del tipo que gobierna: el subyacente si es un
        // enum, y el suyo propio en cualquier otro caso. Es la misma regla que usa la bajada de
        // campos `const`, asi que una constante de `case` y el valor con el que se compara no
        // pueden discrepar de signo. Un `switch` sobre un `enum : byte` con un miembro en 200
        // dependia de esto: con signo, la constante se emitia como -56.
        const frontend::builtins::IntegerTypeInfo* governingEnum =
            findEnumUnderlyingInfo(nameLookup_, governingTypeKey);
        const LLVMBool signExtend =
            (governingEnum != nullptr ? governingEnum->isSigned : isSignedInteger(governingTypeKey))
                ? 1
                : 0;

        const std::size_t sectionCount = node->children.size() - 1;
        std::vector<LLVMBasicBlockRef> sectionBlocks;
        sectionBlocks.reserve(sectionCount);
        for (std::size_t index = 0; index < sectionCount; ++index)
        {
            sectionBlocks.push_back(appendBlock("switch.case"));
        }
        LLVMBasicBlockRef endBlock = appendBlock("switch.end");

        LLVMBasicBlockRef defaultBlock = endBlock;
        unsigned caseCount = 0;
        for (std::size_t index = 0; index < sectionCount; ++index)
        {
            const frontend::ProjectOopMirNode* section = node->children[index + 1].get();
            if (section == nullptr || section->kind != frontend::ProjectOopMirNodeKind::SwitchSection)
                return fail(error, "a switch MIR child is not a section");
            if (section->switchHasDefault) defaultBlock = sectionBlocks[index];
            caseCount += static_cast<unsigned>(section->switchLabels.size());
        }

        LLVMValueRef instruction = LLVMBuildSwitch(builder_, governing, defaultBlock, caseCount);
        for (std::size_t index = 0; index < sectionCount; ++index)
        {
            const frontend::ProjectOopMirNode* section = node->children[index + 1].get();
            for (const std::int64_t label : section->switchLabels)
            {
                LLVMAddCase(
                    instruction,
                    LLVMConstInt(governingType, static_cast<unsigned long long>(label), signExtend),
                    sectionBlocks[index]);
            }
        }

        // The frame that makes `break` mean "leave the switch". Its continue target is INHERITED
        // from the enclosing loop, if there is one, so `continue` inside a case continues that loop
        // -- the body model already proved there is one to continue.
        const LLVMBasicBlockRef enclosingContinue =
            loopStack_.empty() ? nullptr : loopStack_.back().continueTarget;
        const std::size_t enclosingContinueDepth =
            loopStack_.empty() ? 0 : loopStack_.back().continueFuncletDepth;
        loopStack_.push_back(
            {endBlock, enclosingContinue, funcletStack_.size(), enclosingContinueDepth, {}, nullptr});

        bool ok = true;
        for (std::size_t index = 0; index < sectionCount && ok; ++index)
        {
            const frontend::ProjectOopMirNode* section = node->children[index + 1].get();
            LLVMPositionBuilderAtEnd(builder_, sectionBlocks[index]);
            if (section->children.empty() || section->children.front() == nullptr)
            {
                // Labels with no statements. Legal, and it means "match these values and do
                // nothing" -- never "fall into the next section".
                LLVMBuildBr(builder_, endBlock);
                continue;
            }
            ok = lowerStatement(section->children.front().get(), error);
            if (ok && !terminated()) LLVMBuildBr(builder_, endBlock);
        }
        loopStack_.pop_back();
        if (!ok) return false;

        LLVMPositionBuilderAtEnd(builder_, endBlock);
        return true;
    }

bool OopLlvmImplementation::lowerFor(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 4) return fail(error, "Stage 34 for MIR is incomplete");
        // The initializer local belongs to the `for`, not to the surrounding brace. Without a
        // loop scope, two sequential `for (int i...)` declarations become sibling S_LOCAL records
        // with one identical function-wide S_DEFRANGE, and a debugger may show the dead `i`.
        LLVMMetadataRef lexicalScope = createDebugLexicalScope(node, currentDebugScope_);
        ScopedDebugScope scopedDebugScope(currentDebugScope_, lexicalScope);
        const std::size_t cleanupDepthAtEntry = activeCleanupRegions_.size();
        if (node->children[0] != nullptr)
        {
            if (node->children[0]->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
                node->children[0]->kind == frontend::ProjectOopMirNodeKind::Block)
            {
                if (!lowerStatement(node->children[0].get(), error)) return false;
                if (node->children[0]->cleanupRegionOrdinal.has_value())
                    activeCleanupRegions_.push_back(*node->children[0]->cleanupRegionOrdinal);
            }
            else if (lowerExpression(node->children[0].get(), error) == nullptr) return false;
            if (terminated()) return true;
        }
        LLVMBasicBlockRef condBlock = appendBlock("for.cond");
        LLVMBasicBlockRef bodyBlock = appendBlock("for.body");
        LLVMBasicBlockRef incBlock = appendBlock("for.inc");
        LLVMBasicBlockRef endBlock = appendBlock("for.end");
        LLVMBuildBr(builder_, condBlock);
        LLVMPositionBuilderAtEnd(builder_, condBlock);
        if (node->children[1] != nullptr)
        {
            LLVMValueRef condition = lowerExpression(node->children[1].get(), error);
            if (condition == nullptr) return false;
            if (!terminated()) LLVMBuildCondBr(builder_, condition, bodyBlock, endBlock);
        }
        else LLVMBuildBr(builder_, bodyBlock);

        loopStack_.push_back({
            endBlock, incBlock, funcletStack_.size(), funcletStack_.size(), {}, nullptr});
        LLVMPositionBuilderAtEnd(builder_, bodyBlock);
        const bool bodyOk = lowerStatement(node->children[3].get(), error);
        if (bodyOk && !terminated()) LLVMBuildBr(builder_, incBlock);
        loopStack_.pop_back();
        if (!bodyOk) return false;

        LLVMPositionBuilderAtEnd(builder_, incBlock);
        if (node->children[2] != nullptr)
        {
            if (node->children[2]->kind == frontend::ProjectOopMirNodeKind::Block)
            {
                if (!lowerStatement(node->children[2].get(), error)) return false;
            }
            else if (lowerExpression(node->children[2].get(), error) == nullptr)
            {
                return false;
            }
        }
        if (!terminated()) LLVMBuildBr(builder_, condBlock);
        LLVMPositionBuilderAtEnd(builder_, endBlock);
        if (!terminated())
        {
            while (activeCleanupRegions_.size() > cleanupDepthAtEntry)
            {
                const std::size_t ordinal = activeCleanupRegions_.back();
                activeCleanupRegions_.pop_back();
                if (!emitCleanupAction(ordinal, error))
                {
                    activeCleanupRegions_.resize(cleanupDepthAtEntry);
                    return false;
                }
            }
        }
        activeCleanupRegions_.resize(cleanupDepthAtEntry);
        return true;
    }

}
