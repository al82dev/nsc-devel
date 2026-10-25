#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::resolveStructuredExitTarget(
        const frontend::ProjectOopMirNode* node,
        bool isBreak,
        bool isContinue,
        std::size_t& targetLoopIndex,
        std::size_t& targetFuncletDepth,
        std::string& error)
{
        if (!isBreak && !isContinue) return true;
        if (loopStack_.empty()) return fail(error, "Stage 45.10 loop exit has no LLVM loop target");
        if (!node->stringValue.empty())
        {
            bool found = false;
            for (std::size_t index = loopStack_.size(); index > 0; --index)
            {
                if (loopStack_[index - 1U].label != node->stringValue) continue;
                targetLoopIndex = index - 1U;
                found = true;
                break;
            }
            if (!found) return fail(error, "T07-F05 labelled loop exit lost its LLVM target");
        }
        // A switch frame carries its own break target but the ENCLOSING LOOP's continue
        // target, so the two sides can sit at different funclet depths. Picking the wrong one
        // here would make a break unwind funclets it must not, or a continue skip funclets it
        // must.
        targetFuncletDepth = isBreak
            ? loopStack_[targetLoopIndex].funcletDepth
            : loopStack_[targetLoopIndex].continueFuncletDepth;
        if (targetFuncletDepth > funcletStack_.size())
            return fail(error, "Stage 45.10 loop target has an invalid funclet depth");
        if (isContinue && loopStack_[targetLoopIndex].continueTarget == nullptr)
            return fail(error, "continue reached the backend with no enclosing loop target");
        return true;
    }

bool OopLlvmImplementation::emitSimpleStructuredExit(
        bool isReturn,
        bool isBreak,
        std::size_t targetLoopIndex,
        LLVMValueRef returnValue,
        std::string& error)
{
        if (isReturn)
        {
            releaseOwnedCaptureCells();
            if (!emitRuntimeDebugTrace(
                    1U, runtimeDebugCallableRecord(*currentMirFunction_, "EXIT"), error)) return false;
            if (returnValue == nullptr) LLVMBuildRetVoid(builder_);
            else emitAbiReturn(returnValue);
        }
        else if (isBreak) LLVMBuildBr(builder_, loopStack_[targetLoopIndex].breakTarget);
        else LLVMBuildBr(builder_, loopStack_[targetLoopIndex].continueTarget);
        return true;
    }

bool OopLlvmImplementation::emitStructuredExitFunclets(
        const frontend::ProjectOopMirNode* node,
        std::size_t targetFuncletDepth,
        std::size_t& pathIndex,
        std::string& error)
{
        while (funcletStack_.size() > targetFuncletDepth)
        {
            const FuncletContext current = funcletStack_.back();
            if (current.kind != FuncletKind::Catch)
            {
                return fail(error,
                    "Stage 45.10 semantic invariant forbids structured exit across an active cleanup funclet");
            }
            while (pathIndex < node->cleanupRegionOrdinals.size())
            {
                const std::optional<std::size_t> index = activeCleanupIndex(
                    node->cleanupRegionOrdinals[pathIndex], activeCleanupRegions_);
                if (!index.has_value())
                    return fail(error, "Stage 45.10 cleanup path lost lexical cleanup state");
                if (*index < current.cleanupDepthAtEntry) break;
                if (!emitOneStructuredCleanup(node->cleanupRegionOrdinals[pathIndex], error))
                    return false;
                ++pathIndex;
                if (terminated()) return true;
            }
            // NOT REACHED TODAY, and left standing deliberately. Since the B-006/B-007 fix the
            // only push site for funcletStack_ builds FuncletKind::Cleanup entries, so the guard
            // above always takes the failure branch and no exit ever unwinds a Catch funclet here.
            // Verified by sweeping every push_back into funcletStack_, not assumed. It stays
            // because it is the correct code for a Catch entry should one ever be pushed again --
            // and if that happens the extra clear is harmless: __nativesharp_exception_clear only
            // stores null into one TLS slot, so the EH-001 clear emitted above and this one are
            // idempotent with respect to each other. That is what makes "clear twice" a non-issue
            // rather than a bug to guard against.
            LLVMBasicBlockRef catchExit = appendBlock("eh.catch.exit");
            LLVMBuildCatchRet(builder_, current.pad, catchExit);
            funcletStack_.pop_back();
            enterBlock(catchExit);
            buildNoUnwindCall(exceptionClearFunctionType_, exceptionClearFunction_, nullptr, 0, "");
        }
        return true;
    }

bool OopLlvmImplementation::emitRemainingStructuredCleanups(
        const frontend::ProjectOopMirNode* node,
        std::size_t& pathIndex,
        std::string& error)
{
        if (terminated()) return true;
        while (pathIndex < node->cleanupRegionOrdinals.size())
        {
            if (!emitOneStructuredCleanup(node->cleanupRegionOrdinals[pathIndex], error)) return false;
            ++pathIndex;
            if (terminated()) return true;
        }
        return true;
    }

bool OopLlvmImplementation::finishComplexStructuredExit(
        bool isReturn,
        bool isBreak,
        std::size_t targetLoopIndex,
        LLVMValueRef returnValue,
        std::string& error)
{
        if (!isReturn)
        {
            LLVMBuildBr(builder_, isBreak
                ? loopStack_[targetLoopIndex].breakTarget
                : loopStack_[targetLoopIndex].continueTarget);
            return true;
        }
        releaseOwnedCaptureCells();
        if (returnValue == nullptr)
        {
            if (!emitRuntimeDebugTrace(
                    1U, runtimeDebugCallableRecord(*currentMirFunction_, "EXIT"), error)) return false;
            LLVMBuildRetVoid(builder_);
            return true;
        }
        LLVMTypeRef returnType = currentMirFunction_->referenceReturn
            ? LLVMPointerTypeInContext(context_, 0)
            : llvmType(currentMirFunction_->returnTypeKey, error);
        if (returnType == nullptr) return false;
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, returnType, returnStorage_, "eh.return.value");
        if (!emitRuntimeDebugTrace(
                1U, runtimeDebugCallableRecord(*currentMirFunction_, "EXIT"), error)) return false;
        emitAbiReturn(loaded);
        return true;
    }

bool OopLlvmImplementation::lowerStructuredExit(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef returnValueOverride )
{
        const bool isReturn = node->kind == frontend::ProjectOopMirNodeKind::Return;
        const bool isBreak = node->kind == frontend::ProjectOopMirNodeKind::Break;
        const bool isContinue = node->kind == frontend::ProjectOopMirNodeKind::Continue;
        if (!isReturn && !isBreak && !isContinue)
            return fail(error, "Stage 45.10 structured-exit helper received a non-exit MIR node");

        std::size_t targetFuncletDepth = 0;
        std::size_t targetLoopIndex = loopStack_.empty() ? 0U : loopStack_.size() - 1U;
        if (!resolveStructuredExitTarget(
                node, isBreak, isContinue, targetLoopIndex, targetFuncletDepth, error)) return false;

        LLVMValueRef returnValue = returnValueOverride;
        if (isReturn && returnValue == nullptr && !node->children.empty())
        {
            returnValue = lowerExpression(node->children.front().get(), error);
            if (returnValue == nullptr) return false;
            // The value expression may itself diverge (`return panic()`). Its terminator is the
            // complete return path; emitting a second return, cleanup branch, or spill after it
            // would create invalid IR and a fictitious value.
            if (terminated()) return true;
        }
        if (isBreak && !node->children.empty())
        {
            if (node->children.size() != 1 || node->children.front() == nullptr)
                return fail(error, "T07-F05 break value contract is malformed");
            LLVMValueRef breakValue = lowerExpression(node->children.front().get(), error);
            if (breakValue == nullptr) return false;
            if (terminated()) return true;
            LLVMValueRef storage = loopStack_[targetLoopIndex].resultStorage;
            if (storage == nullptr)
                return fail(error, "T07-F05 value break targets a non-value loop");
            LLVMBuildStore(builder_, breakValue, storage);
        }

        const bool complexExit = !node->cleanupRegionOrdinals.empty() ||
            funcletStack_.size() > targetFuncletDepth;
        if (isReturn && returnValue != nullptr && complexExit)
        {
            if (returnStorage_ == nullptr)
                return fail(error, "Stage 45.10 return through cleanup/catch has no return spill slot");
            LLVMBuildStore(builder_, returnValue, returnStorage_);
        }

        // CC1 EH-001. Before ANY of the exit shapes below -- simple branch, cleanup traversal,
        // funclet unwind -- and on both paths equally, because the divergence being closed is
        // precisely that the simple one skipped it. Emitted BEFORE the cleanups rather than after,
        // matching what eh.catch.handled.N already does (it clears, then branches into the
        // `finally`): a destructor running on the way out may itself throw and publish a new
        // current exception, and clearing afterwards would wipe that one instead of this one.
        if (structuredExitLeavesCatchBody(isReturn, targetLoopIndex))
        {
            emitCatchBodyExceptionClear();
        }

        if (!complexExit)
            return emitSimpleStructuredExit(
                isReturn, isBreak, targetLoopIndex, returnValue, error);

        const std::vector<std::size_t> savedActive = activeCleanupRegions_;
        const std::vector<FuncletContext> savedFunclets = funcletStack_;
        std::size_t pathIndex = 0;
        auto restoreState = [&]()
        {
            activeCleanupRegions_ = savedActive;
            funcletStack_ = savedFunclets;
        };

        if (!emitStructuredExitFunclets(
                node, targetFuncletDepth, pathIndex, error) ||
            !emitRemainingStructuredCleanups(node, pathIndex, error))
        {
            restoreState();
            return false;
        }
        if (terminated())
        {
            restoreState();
            return true;
        }
        if (!finishComplexStructuredExit(
                isReturn, isBreak, targetLoopIndex, returnValue, error))
        {
            restoreState();
            return false;
        }
        restoreState();
        return true;
    }

bool OopLlvmImplementation::lowerUsingRegion(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (!node->cleanupRegionOrdinal.has_value() || !node->localOrdinal.has_value() ||
            node->children.size() < 2 || node->children[0] == nullptr || node->children[1] == nullptr)
        {
            return fail(error, "Stage 45.10 using MIR region is incomplete");
        }
        const auto localFound = localStorage_.find(*node->localOrdinal);
        if (localFound == localStorage_.end() || localFound->second == nullptr)
            return fail(error, "Stage 45.10 using MIR has no allocated resource local");
        if (!emitDebugLocal(node, localFound->second, error)) return false;

        LLVMValueRef initializer = lowerExpression(node->children[0].get(), error);
        if (initializer == nullptr) return false;
        LLVMBuildStore(builder_, initializer, localFound->second);

        activeCleanupRegions_.push_back(*node->cleanupRegionOrdinal);
        const bool bodyOk = lowerStatement(node->children[1].get(), error);
        activeCleanupRegions_.pop_back();
        if (!bodyOk) return false;
        if (!terminated())
        {
            if (!emitCleanupAction(*node->cleanupRegionOrdinal, error)) return false;
        }
        return true;
    }

bool OopLlvmImplementation::lowerExceptionalTransfer(
        LLVMValueRef exception,
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        LLVMBasicBlockRef finalTarget = nullptr;
        bool targetIsLocalToCurrentFunclet = false;
        if (node->exceptionTargetRegionOrdinal.has_value())
        {
            const auto found = exceptionDispatchBlocks_.find(*node->exceptionTargetRegionOrdinal);
            if (found == exceptionDispatchBlocks_.end())
                return fail(error, "Stage 45.10 exception transfer references an inactive MIR handler");
            finalTarget = found->second;
            for (auto handler = exceptionHandlerStack_.rbegin();
                 handler != exceptionHandlerStack_.rend(); ++handler)
            {
                if (handler->regionOrdinal == *node->exceptionTargetRegionOrdinal)
                {
                    targetIsLocalToCurrentFunclet =
                        handler->funcletDepthAtEntry == funcletStack_.size();
                    break;
                }
            }
        }

        std::vector<std::size_t> cleanupPath = node->cleanupRegionOrdinals;
        if (!funcletStack_.empty() && !targetIsLocalToCurrentFunclet)
        {
            const std::size_t boundaryDepth = funcletStack_.back().cleanupDepthAtEntry;
            cleanupPath.erase(
                std::remove_if(
                    cleanupPath.begin(), cleanupPath.end(),
                    [&](std::size_t ordinal)
                    {
                        const std::optional<std::size_t> index =
                            activeCleanupIndex(ordinal, activeCleanupRegions_);
                        return !index.has_value() || *index < boundaryDepth;
                    }),
                cleanupPath.end());
            finalTarget = funcletStack_.back().unwindDestination;
        }

        if (!node->exceptionTargetRegionOrdinal.has_value() && finalTarget == nullptr &&
            !ownedCaptureCells_.empty())
        {
            finalTarget = captureCellExceptionalCleanup(error);
            if (finalTarget == nullptr) return false;
        }

        LLVMBasicBlockRef unwindTarget = cleanupPath.empty()
            ? finalTarget
            : emitExceptionalCleanupChain(cleanupPath, finalTarget, error);
        if (!error.empty()) return false;

        LLVMValueRef arguments[] = {exception};
        if (unwindTarget != nullptr)
        {
            LLVMBasicBlockRef normalBlock = appendBlock("eh.throw.unreachable");
            LLVMValueRef invoke = buildInvokeInCurrentFunclet(
                nativeThrowFunctionType_, nativeThrowFunction_, arguments, 1,
                normalBlock, unwindTarget, "");
            if (invoke == nullptr) return fail(error, "Stage 45.10 failed to create exceptional invoke");
            enterBlock(normalBlock);
            // This block is unreachable in principle -- __nativesharp_throw ends in RaiseException
            // and never returns -- but it must NOT be left as a bare `unreachable`, which emits
            // ZERO bytes. Bug found by real execution (see
            // tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp scenario 14):
            //
            // `__C_specific_handler` decides whether a `__finally` scope is being exited by testing
            // whether the unwind TargetIp lies inside that scope's [Begin, End) instruction range,
            // where End is "last protected instruction + 1". When the last protected instruction is
            // the throw call and this block emits nothing, the catch handler laid out next starts
            // at exactly that End-1 address -- inside the range -- so the personality concludes the
            // enclosing scope is NOT being left and silently skips its `finally`. At -O0 unrelated
            // instructions happened to sit in between and hid it; at -O2 they do not.
            //
            // Verified against clang: for the equivalent C SEH nesting clang always has real
            // normal-path code (the finally call plus the epilogue) between the protected range and
            // the handler, so its TargetIp lands outside the range and its finally runs. Emitting a
            // real trap here restores that property -- and, on the impossible edge where the throw
            // does return, fails loudly instead of falling into the handler.
            emitThrowReturnedTrap();
            return true;
        }

        buildNoUnwindCall(
            nativeThrowFunctionType_, nativeThrowFunction_, arguments, 1, "");
        // Same reason as the invoke path above, and just as load-bearing. This is the branch taken
        // when the throw has no unwind destination of its own -- typically a small method whose
        // only statement is `throw`. LLVM's INLINER then turns this plain call into an `invoke` at
        // any call site that does have one, synthesising an empty `.noexc` continuation block; if
        // nothing follows the call here, that block emits zero bytes and the enclosing scope's
        // `finally` (or `owned` parameter cleanup) is silently skipped at -O2 by exactly the
        // TargetIp-inside-the-range rule described above. Emitting the trap on BOTH branches is
        // what makes the property hold after inlining, not just before it.
        emitThrowReturnedTrap();
        return true;
    }

void OopLlvmImplementation::emitThrowReturnedTrap()
{
        LLVMValueRef trapReason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::ThrowReturned),
            0);
        LLVMValueRef trapArguments[] = {trapReason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
    }

bool OopLlvmImplementation::lowerNativePanicTransfer(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef* arguments,
        std::string& error)
{
        std::vector<std::size_t> cleanupPath = node->cleanupRegionOrdinals;
        LLVMBasicBlockRef finalTarget = nullptr;
        if (!funcletStack_.empty())
        {
            const std::size_t boundaryDepth = funcletStack_.back().cleanupDepthAtEntry;
            cleanupPath.erase(
                std::remove_if(
                    cleanupPath.begin(), cleanupPath.end(),
                    [&](std::size_t ordinal)
                    {
                        const std::optional<std::size_t> index =
                            activeCleanupIndex(ordinal, activeCleanupRegions_);
                        return !index.has_value() || *index < boundaryDepth;
                    }),
                cleanupPath.end());
            finalTarget = funcletStack_.back().unwindDestination;
        }
        else if (staticInitializationFailureUnwindBlock_ != nullptr)
        {
            finalTarget = staticInitializationFailureUnwindBlock_;
        }
        if (finalTarget == nullptr && !ownedCaptureCells_.empty())
        {
            finalTarget = captureCellExceptionalCleanup(error);
            if (finalTarget == nullptr) return false;
        }
        LLVMBasicBlockRef unwindTarget = cleanupPath.empty()
            ? finalTarget
            : emitExceptionalCleanupChain(cleanupPath, finalTarget, error);
        if (!error.empty()) return false;

        if (unwindTarget != nullptr)
        {
            LLVMBasicBlockRef normalBlock = appendBlock("panic.unexpected-return");
            LLVMValueRef invoke = buildInvokeInCurrentFunclet(
                raiseExceptionFunctionType_, raiseExceptionFunction_, arguments, 4,
                normalBlock, unwindTarget, "");
            if (invoke == nullptr)
                return fail(error, "T07-F06 failed to create native panic invoke");
            enterBlock(normalBlock);
        }
        else
        {
            buildNoUnwindCall(
                raiseExceptionFunctionType_, raiseExceptionFunction_, arguments, 4, "");
        }
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::Panic), 0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
        return true;
    }

bool OopLlvmImplementation::lowerRethrow(const frontend::ProjectOopMirNode* node, std::string& error)
{
        // Multi-catch fix: catch bodies are ordinary parent code now, not funclet interiors, so
        // the innermost enclosing catch's exception object lives on activeCatchExceptions_ rather
        // than on a Catch entry in funcletStack_. The funcletStack_ walk is kept as a fallback so
        // any remaining path that still lowers a handler inside a funclet keeps working unchanged.
        LLVMValueRef exception =
            activeCatchExceptions_.empty() ? nullptr : activeCatchExceptions_.back().exception;
        for (auto found = funcletStack_.rbegin();
             exception == nullptr && found != funcletStack_.rend(); ++found)
        {
            if (found->kind == FuncletKind::Catch && found->caughtException != nullptr)
            {
                exception = found->caughtException;
                break;
            }
        }
        if (exception == nullptr)
            return fail(error, "Stage 45.10 rethrow has no active caught NativeSharp exception");
        return lowerExceptionalTransfer(exception, node, error);
    }

}
