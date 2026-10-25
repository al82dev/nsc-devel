#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::lowerStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr) return true;
        ScopedDebugLocation scopedDebugLocation(builder_, debugLocation(node));
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::Block:
                return lowerBlockStatement(node, error);
            case frontend::ProjectOopMirNodeKind::OwnedParameterCleanup:
                return true;
            case frontend::ProjectOopMirNodeKind::GeneratorFrameInit:
                return lowerGeneratorFrameInitStatement(node, error);
            case frontend::ProjectOopMirNodeKind::LocalDeclaration:
                return lowerLocalDeclarationStatement(node, error);
            case frontend::ProjectOopMirNodeKind::ExpressionStatement:
                return lowerExpressionStatement(node, error);
            case frontend::ProjectOopMirNodeKind::InlineAssembly:
                return lowerInlineAssembly(node, error);
            case frontend::ProjectOopMirNodeKind::Return:
            case frontend::ProjectOopMirNodeKind::Break:
            case frontend::ProjectOopMirNodeKind::Continue:
                return lowerStructuredExit(node, error);
            case frontend::ProjectOopMirNodeKind::TryRegion:
                return lowerTryRegion(node, error);
            case frontend::ProjectOopMirNodeKind::CatchRegion:
                return fail(error, "Stage 45.10 catch-region must be lowered by its enclosing try-region");
            case frontend::ProjectOopMirNodeKind::FinallyRegion:
                return fail(error, "Stage 45.10 finally-region must be lowered by its owning try/cleanup path");
            case frontend::ProjectOopMirNodeKind::UsingRegion:
                return lowerUsingRegion(node, error);
            case frontend::ProjectOopMirNodeKind::CleanupContinuation:
                return true;
            case frontend::ProjectOopMirNodeKind::Throw:
                return lowerThrow(node, error);
            case frontend::ProjectOopMirNodeKind::Rethrow:
                return lowerRethrow(node, error);
            case frontend::ProjectOopMirNodeKind::If:
                return lowerIf(node, error);
            case frontend::ProjectOopMirNodeKind::While:
                return lowerWhile(node, error);
            case frontend::ProjectOopMirNodeKind::DoWhile:
                return lowerDoWhile(node, error);
            case frontend::ProjectOopMirNodeKind::For:
                return lowerFor(node, error);
            case frontend::ProjectOopMirNodeKind::Switch:
                return lowerSwitch(node, error);
            case frontend::ProjectOopMirNodeKind::SwitchSection:
                return fail(error, "a switch section must be lowered by its enclosing switch");
            case frontend::ProjectOopMirNodeKind::SuspendPoint:
                return lowerSuspendPointStatement(node, error);
            case frontend::ProjectOopMirNodeKind::YieldValue:
                return lowerYieldValueStatement(node, error);
            case frontend::ProjectOopMirNodeKind::YieldBreak:
                return lowerYieldBreakStatement(node, error);
            case frontend::ProjectOopMirNodeKind::AsyncYieldValue:
                return lowerAsyncYieldValueStatement(node, error);
            case frontend::ProjectOopMirNodeKind::DropFrame:
                return lowerDropFrameStatement(node, error);
            default:
            {
                LLVMValueRef value = lowerExpression(node, error);
                return value != nullptr && error.empty();
            }
        }
}

bool OopLlvmImplementation::lowerExpressionStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (!node->children.empty() &&
            lowerExpression(node->children.front().get(), error) == nullptr &&
            error.empty())
            return fail(error, "Stage 34 expression statement produced no value");
        return error.empty();
}

bool OopLlvmImplementation::lowerBlockStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
                LLVMMetadataRef lexicalScope =
                    createDebugLexicalScope(node, currentDebugScope_);
                ScopedDebugScope scopedDebugScope(currentDebugScope_, lexicalScope);
                const std::size_t cleanupDepthAtEntry = activeCleanupRegions_.size();
                for (const auto& child : node->children)
                {
                    if (terminated()) break;
                    // A source block alone is too broad for a local: CodeView treats the
                    // variable's lexical parent as its visibility range, so assigning every
                    // declaration to the brace scope makes a local visible before its own line.
                    // Nest one scope at each declaration and keep it active for the remainder of
                    // this block. Consecutive declarations therefore form the same lifetime
                    // chain as the source language: earlier locals remain visible, later locals
                    // begin exactly where declared, and all scopes close at the brace.
                    //
                    // ES: El inicializador se baja todavía en el ámbito padre y sólo la variable
                    // pertenece al ámbito de la declaración, que se activa al terminar la sentencia.
                    // Un local no se ve en su propia línea (su valor aún no existe: en un bucle
                    // sería el de la vuelta anterior); se ve desde la sentencia siguiente.
                    // EN: The initializer is still lowered in the parent scope and only the
                    // variable belongs to the declaration scope, which becomes active once the
                    // statement ends. A local is not visible on its own line (its value does not
                    // exist yet; in a loop it would be the previous iteration's); it is visible from
                    // the next statement on.
                    LLVMMetadataRef declarationScope = nullptr;
                    if (child != nullptr &&
                        child->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration)
                    {
                        declarationScope = createDebugLexicalScope(child.get(), currentDebugScope_);
                    }
                    LLVMMetadataRef enclosingPendingScope = pendingLocalDebugScope_;
                    pendingLocalDebugScope_ = declarationScope;
                    const bool lowered = lowerStatement(child.get(), error);
                    pendingLocalDebugScope_ = enclosingPendingScope;
                    if (declarationScope != nullptr) currentDebugScope_ = declarationScope;
                    if (!lowered)
                    {
                        activeCleanupRegions_.resize(cleanupDepthAtEntry);
                        return false;
                    }
                    if (child != nullptr &&
                        (child->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
                         child->kind == frontend::ProjectOopMirNodeKind::OwnedParameterCleanup) &&
                        child->cleanupRegionOrdinal.has_value())
                    {
                        activeCleanupRegions_.push_back(*child->cleanupRegionOrdinal);
                    }
                }
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
                        if (terminated()) break;
                    }
                }
                activeCleanupRegions_.resize(cleanupDepthAtEntry);
                return true;
}

bool OopLlvmImplementation::lowerGeneratorFrameInitStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
                // NS304-T06-F04: allocate the frame, store the captured parameters, and set
                // self.frame (after the `self` default initializer, which zeroed the struct).
                if (!node->localOrdinal.has_value())
                    return fail(error, "generator frame init missing self local ordinal");
                const auto selfFound = localStorage_.find(*node->localOrdinal);
                if (selfFound == localStorage_.end() || selfFound->second == nullptr)
                    return fail(error, "generator frame init self storage missing");
                LLVMValueRef selfAlloca = selfFound->second;
                if (currentMirFunction_ == nullptr)
                    return fail(error, "generator frame init missing current function");
                LLVMValueRef size = LLVMConstInt(
                    LLVMInt64TypeInContext(context_), currentMirFunction_->asyncFrameSize, 0);
                LLVMValueRef allocArgs[] = {size};
                LLVMValueRef frame = buildNoUnwindCall(
                    allocationFunctionType_, allocationFunction_, allocArgs, 1, "gen.frame.alloc");
                if (frame == nullptr)
                    return fail(error, "generator constructor frame allocation failed");
                LLVMBuildStore(builder_, frame, byteOffset(selfAlloca, 8, "gen.self.frame"));
                for (const auto& [paramOrdinal, offset, typeKey] :
                     currentMirFunction_->asyncFrameParameters)
                {
                    const auto paramFound = parameterStorage_.find(paramOrdinal);
                    if (paramFound == parameterStorage_.end() || paramFound->second == nullptr) continue;
                    LLVMTypeRef paramType = llvmType(typeKey, error);
                    if (paramType == nullptr) return false;
                    LLVMBuildStore(
                        builder_,
                        LLVMBuildLoad2(builder_, paramType, paramFound->second, "gen.param"),
                        byteOffset(frame, offset, "gen.frame.param"));
                }
                return true;
}

bool OopLlvmImplementation::lowerLocalDeclarationStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
                if (node->typeKey == "builtin:never")
                {
                    if (node->children.size() != 1 || node->children.front() == nullptr)
                        return fail(error, "T07 never local requires one divergent initializer");
                    LLVMValueRef ignored = lowerExpression(node->children.front().get(), error);
                    if (ignored == nullptr) return false;
                    if (!terminated())
                        return fail(error, "T07 never local initializer unexpectedly produced a value");
                    return true;
                }
                std::optional<StaticLocalGuard> staticGuard;
                if (node->staticLocal)
                {
                    staticGuard = beginStaticLocalInitialization(node, error);
                    if (!staticGuard.has_value()) return false;
                }
                bool initialized = true;
                LLVMValueRef localAddress = nullptr;
                if (node->localOrdinal.has_value())
                {
                    const auto storage = localStorage_.find(*node->localOrdinal);
                    if (storage != localStorage_.end()) localAddress = storage->second;
                }
                if (!node->children.empty() && node->children.front() != nullptr)
                {
                    LLVMValueRef address = localAddress;
                    if (address == nullptr)
                        return fail(error, "Stage 34 local initialization has no storage");
                    if (node->children.front()->kind ==
                        frontend::ProjectOopMirNodeKind::AggregateInitialize)
                    {
                        if (!initializeAggregateLocalDeclaration(
                                node, address, initialized, error)) return false;
                    }
                    else
                    {
                        LLVMValueRef value = lowerExpression(node->children.front().get(), error);
                        if (value == nullptr)
                        {
                            staticInitializationFailureUnwindBlock_ = nullptr;
                            const std::string cause = error;
                            return fail(
                                error,
                                "Stage 34 local initialization failed" +
                                    (cause.empty() ? std::string() : ": " + cause));
                        }
                        // `T local = panic()` type-checks through the bottom conversion but never
                        // initializes storage. Do not store the lowering sentinel or mark a drop
                        // flag/debug lifetime after the block has terminated.
                        if (terminated())
                        {
                            if (!staticGuard.has_value()) return true;
                        }
                        else
                        {
                            LLVMBuildStore(builder_, value, address);
                        }
                    }
                }
                else
                {
                    initialized = emitDefaultLocalStorage(node, error);
                }
                if (!initialized)
                {
                    staticInitializationFailureUnwindBlock_ = nullptr;
                    return false;
                }
                if (node->localOrdinal.has_value())
                {
                    const auto dropFlag = localValueDropFlags_.find(*node->localOrdinal);
                    if (dropFlag != localValueDropFlags_.end())
                    {
                        LLVMBuildStore(
                            builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0),
                            dropFlag->second);
                    }
                }
                if (staticGuard.has_value() &&
                    !finishStaticLocalInitialization(*staticGuard, error)) return false;

                // A local becomes debugger-visible only after its initializer has completed.
                // Emitting dbg.declare before the store lets CodeView extend the location into
                // the preceding source statement when both occupy one machine block.
                if (node->localOrdinal.has_value())
                {
                    const auto storage = localStorage_.find(*node->localOrdinal);
                    if (storage != localStorage_.end() &&
                        !emitDebugLocal(node, storage->second, error))
                        return false;
                }
                return true;
}

bool OopLlvmImplementation::initializeAggregateLocalDeclaration(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        bool& initialized,
        std::string& error)
{
        // T01-F05: a declared aggregate already owns its final stack slot. Fill
        // that slot directly instead of constructing an aggregate.init temporary,
        // loading it, and storing the carrier a second time. The helper keeps the
        // same textual evaluation order and zero-initialization contract used by
        // aggregate expressions in non-place contexts.
        const bool cleanupDuringInitialization =
            node->cleanupRegionOrdinal.has_value() && memberModel_ != nullptr &&
            memberModel_->typeRequiresValueDrop(node->typeKey) &&
            !memberModel_->typeTransitivelyHasCustomDestructor(node->typeKey);
        if (cleanupDuringInitialization)
        {
            // T02-F06: the slot is zero-initialized before its first user
            // expression. Activate its existing cleanup while fields are filled so
            // an exception after one transferred owner destroys precisely the
            // fields already installed. Custom destructors remain excluded because
            // they are allowed to observe the complete value.
            const auto dropFlag = node->localOrdinal.has_value()
                ? localValueDropFlags_.find(*node->localOrdinal)
                : localValueDropFlags_.end();
            if (dropFlag != localValueDropFlags_.end())
            {
                LLVMBuildStore(
                    builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0),
                    dropFlag->second);
            }
            activeCleanupRegions_.push_back(*node->cleanupRegionOrdinal);
        }
        initialized = initializeAggregateIntoSlot(
            node->children.front().get(), address, error);
        if (cleanupDuringInitialization) activeCleanupRegions_.pop_back();
        return true;
}

bool OopLlvmImplementation::lowerSuspendPointStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
                // NS304-T05-F09: a bare `yield;` suspends the poll once. self is parameter 0.
                if (parameterStorage_.empty())
                    return fail(error, "suspend point has no poll self parameter");
                LLVMValueRef self = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0],
                    "suspend.self");
                LLVMTypeRef intType = llvmType("builtin:int", error);
                if (intType == nullptr) return false;
                LLVMValueRef selfState = LLVMBuildLoad2(builder_, intType, self, "suspend.self.state");
                LLVMValueRef prevState = LLVMConstInt(intType, node->integerValue - 1, 0);
                LLVMBasicBlockRef suspendBlock = appendBlock("suspend.suspend");
                LLVMBasicBlockRef resumeBlock = appendBlock("suspend.resume");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(builder_, LLVMIntEQ, selfState, prevState, "suspend.is.entry"),
                    suspendBlock, resumeBlock);

                const auto& frameLocals = currentMirFunction_ != nullptr
                    ? currentMirFunction_->asyncFrameLocals
                    : std::vector<std::tuple<std::size_t, std::uint64_t, std::string>>{};

                // Suspend: save the live locals into the frame, then state = N and return.
                enterBlock(suspendBlock);
                LLVMValueRef suspendFrame = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(self, 8, "suspend.self.frame.addr"), "suspend.self.frame");
                for (const auto& [localOrdinal, offset, typeKey] : frameLocals)
                {
                    const auto localFound = localStorage_.find(localOrdinal);
                    if (localFound == localStorage_.end() || localFound->second == nullptr) continue;
                    LLVMTypeRef localType = llvmType(typeKey, error);
                    if (localType == nullptr) return false;
                    LLVMBuildStore(
                        builder_,
                        LLVMBuildLoad2(builder_, localType, localFound->second, "suspend.local"),
                        byteOffset(suspendFrame, offset, "suspend.frame.slot"));
                }
                LLVMBuildStore(builder_, LLVMConstInt(intType, node->integerValue, 0), self);
                LLVMBuildRetVoid(builder_);

                // Resume: restore the live locals from the frame and continue.
                enterBlock(resumeBlock);
                LLVMValueRef resumeFrame = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(self, 8, "suspend.self.frame.addr2"), "suspend.self.frame");
                for (const auto& [localOrdinal, offset, typeKey] : frameLocals)
                {
                    const auto localFound = localStorage_.find(localOrdinal);
                    if (localFound == localStorage_.end() || localFound->second == nullptr) continue;
                    LLVMTypeRef localType = llvmType(typeKey, error);
                    if (localType == nullptr) return false;
                    LLVMBuildStore(
                        builder_,
                        LLVMBuildLoad2(
                            builder_, localType, byteOffset(resumeFrame, offset, "suspend.frame.slot"),
                            "suspend.saved"),
                        localFound->second);
                }
                return true;
}

bool OopLlvmImplementation::lowerYieldValueStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
                // NS304-T06-F03: `yield return v` produces the item and returns true on the entry
                // state; on a later state it falls through (the next yield or the terminal return).
                if (parameterStorage_.empty())
                    return fail(error, "yield value has no MoveNext self parameter");
                LLVMValueRef self = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0],
                    "yield.self");
                LLVMTypeRef intType = llvmType("builtin:int", error);
                if (intType == nullptr) return false;
                LLVMValueRef selfState = LLVMBuildLoad2(builder_, intType, self, "yield.self.state");
                LLVMValueRef prevState = LLVMConstInt(intType, node->integerValue - 1, 0);
                LLVMBasicBlockRef entryBlock = appendBlock("yield.entry");
                LLVMBasicBlockRef resumeBlock = appendBlock("yield.resume");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(builder_, LLVMIntEQ, selfState, prevState, "yield.is.entry"),
                    entryBlock, resumeBlock);

                LLVMValueRef framePhi = prepareYieldValueFrame(self, entryBlock, error);
                if (framePhi == nullptr) return false;
                LLVMValueRef item = node->children.empty()
                    ? nullptr : lowerExpression(node->children.front().get(), error);
                if (!node->children.empty() && item == nullptr) return false;
                if (item != nullptr)
                {
                    // NS304-T06-F05: a `ref`/`ref mut` item yields an ADDRESS (the child is a
                    // LocalAddress/FieldAddress), so the item slot stores the pointer itself.
                    const bool itemByReference =
                        node->children.front()->kind == frontend::ProjectOopMirNodeKind::LocalAddress ||
                        node->children.front()->kind == frontend::ProjectOopMirNodeKind::FieldAddress;
                    LLVMTypeRef itemType = itemByReference
                        ? LLVMPointerTypeInContext(context_, 0)
                        : llvmType(node->children.front()->typeKey, error);
                    if (itemType == nullptr) return false;
                    LLVMBuildStore(builder_, item, byteOffset(self, 16, "yield.item"));
                }
                LLVMBuildStore(
                    builder_, LLVMConstInt(intType, node->integerValue, 0), self);
                LLVMBuildRet(builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0));

                // Resume: restore the live locals and fall through to the next yield.
                enterBlock(resumeBlock);
                return restoreYieldValueFrame(self, error);
}

LLVMValueRef OopLlvmImplementation::prepareYieldValueFrame(
        LLVMValueRef self, LLVMBasicBlockRef entryBlock, std::string& error)
{
                const auto& frameLocals = currentMirFunction_ != nullptr
                    ? currentMirFunction_->asyncFrameLocals
                    : std::vector<std::tuple<std::size_t, std::uint64_t, std::string>>{};

                // Entry: allocate the frame once (if null), save live locals, store item + state,
                // return true.
                enterBlock(entryBlock);
                LLVMValueRef frame = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(self, 8, "yield.frame.addr"), "yield.frame");
                LLVMBasicBlockRef haveFrame = appendBlock("yield.have.frame");
                LLVMBasicBlockRef allocFrame = appendBlock("yield.alloc.frame");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(
                        builder_, LLVMIntEQ, frame,
                        LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), "yield.frame.null"),
                    allocFrame, haveFrame);
                enterBlock(allocFrame);
                LLVMTypeRef frameType = LLVMArrayType2(
                    LLVMInt8TypeInContext(context_), currentMirFunction_ != nullptr
                        ? currentMirFunction_->asyncFrameSize : 0);
                LLVMValueRef newFrame = entryBlockAlloca(frameType, 8, "yield.frame.storage");
                LLVMBuildStore(builder_, newFrame, byteOffset(self, 8, "yield.frame.addr2"));
                LLVMBuildBr(builder_, haveFrame);
                enterBlock(haveFrame);
                LLVMValueRef framePhi = LLVMBuildPhi(
                    builder_, LLVMPointerTypeInContext(context_, 0), "yield.frame.phi");
                LLVMValueRef frameIncoming[] = { frame, newFrame };
                LLVMBasicBlockRef frameBlocks[] = { entryBlock, allocFrame };
                LLVMAddIncoming(framePhi, frameIncoming, frameBlocks, 2);
                for (const auto& [localOrdinal, offset, typeKey] : frameLocals)
                {
                    const auto localFound = localStorage_.find(localOrdinal);
                    if (localFound == localStorage_.end() || localFound->second == nullptr) continue;
                    LLVMTypeRef localType = llvmType(typeKey, error);
                    if (localType == nullptr) return nullptr;
                    LLVMBuildStore(
                        builder_,
                        LLVMBuildLoad2(builder_, localType, localFound->second, "yield.local"),
                        byteOffset(framePhi, offset, "yield.frame.slot"));
                }
        return framePhi;
}

bool OopLlvmImplementation::restoreYieldValueFrame(
        LLVMValueRef self, std::string& error)
{
                const auto& frameLocals = currentMirFunction_ != nullptr
                    ? currentMirFunction_->asyncFrameLocals
                    : std::vector<std::tuple<std::size_t, std::uint64_t, std::string>>{};
                LLVMValueRef resumeFrame = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(self, 8, "yield.frame.addr3"), "yield.frame");
                for (const auto& [localOrdinal, offset, typeKey] : frameLocals)
                {
                    const auto localFound = localStorage_.find(localOrdinal);
                    if (localFound == localStorage_.end() || localFound->second == nullptr) continue;
                    LLVMTypeRef localType = llvmType(typeKey, error);
                    if (localType == nullptr) return false;
                    LLVMBuildStore(
                        builder_,
                        LLVMBuildLoad2(
                            builder_, localType, byteOffset(resumeFrame, offset, "yield.frame.slot"),
                            "yield.saved"),
                        localFound->second);
                }
                return true;
}

bool OopLlvmImplementation::lowerYieldBreakStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
                // NS304-T06-F07: `yield break` runs the active cleanup (defer/drop) once, then
                // terminates the generator (state = -1) + return false.
                if (parameterStorage_.empty())
                    return fail(error, "yield break has no MoveNext self parameter");
                for (std::size_t index = node->cleanupRegionOrdinals.size(); index > 0; --index)
                {
                    if (!emitOneStructuredCleanup(node->cleanupRegionOrdinals[index - 1], error))
                        return false;
                    if (terminated()) return true;
                }
                LLVMValueRef self = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0],
                    "yieldbreak.self");
                LLVMTypeRef intType = llvmType("builtin:int", error);
                if (intType == nullptr) return false;
                LLVMBuildStore(builder_, LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1), self);
                LLVMBuildRet(builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
                return true;
}

bool OopLlvmImplementation::lowerAsyncYieldValueStatement(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
                // NS304-T07-F02: the async-generator poll's yield. Entry: allocate the frame once,
                // save live locals, write the item + the demand future's result/state and return;
                // resume: restore the live locals and fall through to the next yield.
                if (parameterStorage_.empty())
                    return fail(error, "async yield has no poll self parameter");
                LLVMValueRef future = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0],
                    "ayield.future");
                LLVMValueRef asyncIter = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(future, 8, "ayield.frame.addr"), "ayield.iter");
                LLVMTypeRef intType = llvmType("builtin:int", error);
                if (intType == nullptr) return false;
                LLVMValueRef iterState = nullptr;
                LLVMBasicBlockRef runBlock = nullptr;
                if (!prepareAsyncYieldRun(
                        node, future, asyncIter, intType, iterState, runBlock, error)) return false;
                if (terminated()) return true;
                LLVMValueRef prevState = LLVMConstInt(intType, node->integerValue - 1, 0);
                LLVMBasicBlockRef entryBlock = appendBlock("ayield.entry");
                LLVMBasicBlockRef resumeBlock = appendBlock("ayield.resume");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(builder_, LLVMIntEQ, iterState, prevState, "ayield.is.entry"),
                    entryBlock, resumeBlock);

                LLVMValueRef frame = prepareAsyncYieldFrame(asyncIter, entryBlock, error);
                if (frame == nullptr) return false;
                LLVMValueRef item = lowerAsyncYieldItem(node, frame, error);
                if (!node->children.empty() && item == nullptr) return false;
                if (item != nullptr)
                    LLVMBuildStore(builder_, item, byteOffset(asyncIter, 16, "ayield.item.addr"));
                LLVMBuildStore(
                    builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0),
                    byteOffset(future, 16, "ayield.result.addr"));
                LLVMBuildStore(
                    builder_, LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1),
                    byteOffset(future, 0, "ayield.state.addr"));
                LLVMBuildStore(
                    builder_, LLVMConstInt(intType, node->integerValue, 0), asyncIter);
                LLVMBuildRetVoid(builder_);

                enterBlock(resumeBlock);
                return restoreAsyncYieldFrame(asyncIter, error);
}

bool OopLlvmImplementation::prepareAsyncYieldRun(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef future,
        LLVMValueRef asyncIterator,
        LLVMTypeRef intType,
        LLVMValueRef& iteratorState,
        LLVMBasicBlockRef& runBlock,
        std::string& error)
{
                iteratorState = LLVMBuildLoad2(
                    builder_, intType, asyncIterator, "ayield.iter.state");
                // NS304-T07-F05: a cancelled async-iterator (state == -3) yields no more items; the
                // poll returns false and parks the iterator at the terminal state.
                LLVMValueRef cancelledConst = LLVMConstInt(intType, static_cast<unsigned long long>(-3), 1);
                LLVMBasicBlockRef cancelledBlock = appendBlock("ayield.cancelled");
                runBlock = appendBlock("ayield.run");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(
                        builder_, LLVMIntEQ, iteratorState, cancelledConst,
                        "ayield.is.cancelled"),
                    cancelledBlock, runBlock);
                enterBlock(cancelledBlock);
                // NS304-T07-F05: a cooperative cancellation runs the active finally/defer once
                // before parking the iterator, exactly like the terminal return.
                // NS304-T17-F09: emitOneStructuredCleanup mutates the SHARED lowerer state
                // (activeCleanupRegions_ resize, and a finally body may touch funcletStack_).
                // The cancelled path is a separate LLVM branch: the entry/resume paths follow
                // below and the sibling statements after this yield are lowered next, so that
                // mutation must not leak into them. Snapshot and restore, exactly like
                // lowerStructuredExit. Without this, a second (conditional) yield inside the
                // same try/finally found its cleanup regions already popped by the first
                // yield's cancelled branch and failed with "Stage 45.10 structured exit
                // references an inactive cleanup region".
                const std::vector<std::size_t> savedCancelledActive = activeCleanupRegions_;
                const std::vector<FuncletContext> savedCancelledFunclets = funcletStack_;
                for (std::size_t index = node->cleanupRegionOrdinals.size(); index > 0; --index)
                {
                    if (!emitOneStructuredCleanup(node->cleanupRegionOrdinals[index - 1], error))
                    {
                        activeCleanupRegions_ = savedCancelledActive;
                        funcletStack_ = savedCancelledFunclets;
                        return false;
                    }
                    if (terminated())
                    {
                        activeCleanupRegions_ = savedCancelledActive;
                        funcletStack_ = savedCancelledFunclets;
                        return true;
                    }
                }
                        activeCleanupRegions_ = savedCancelledActive;
                        funcletStack_ = savedCancelledFunclets;
                LLVMBuildStore(
                    builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0),
                    byteOffset(future, 16, "ayield.cancelled.result"));
                LLVMBuildStore(
                    builder_, LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1),
                    byteOffset(future, 0, "ayield.cancelled.state"));
                LLVMBuildStore(
                    builder_, LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1), asyncIterator);
                LLVMBuildRetVoid(builder_);
                enterBlock(runBlock);
                return true;
}

LLVMValueRef OopLlvmImplementation::prepareAsyncYieldFrame(
        LLVMValueRef asyncIterator, LLVMBasicBlockRef entryBlock, std::string& error)
{
                const auto& frameLocals = currentMirFunction_ != nullptr
                    ? currentMirFunction_->asyncFrameLocals
                    : std::vector<std::tuple<std::size_t, std::uint64_t, std::string>>{};

                enterBlock(entryBlock);
                // NS304-T07-F04: the async-generator's locals live in the async-iterator's PERSISTENT
                // frame (allocated by the constructor via GeneratorFrameInit). A null frame here is an
                // internal invariant violation, never recoverable with a stack alloca.
                LLVMValueRef frame = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(asyncIterator, 8, "ayield.frame.slot.addr"), "ayield.frame");
                LLVMBasicBlockRef frameOk = appendBlock("ayield.frame.ok");
                LLVMBasicBlockRef frameTrap = appendBlock("ayield.frame.trap");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(
                        builder_, LLVMIntEQ, frame,
                        LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), "ayield.frame.null"),
                    frameTrap, frameOk);
                enterBlock(frameTrap);
                if (trapFunction_ == nullptr || trapFunctionType_ == nullptr)
                    return failValue(error, "async yield frame invariant trap is unavailable");
                {
                    LLVMValueRef trapReason = LLVMConstInt(
                        LLVMInt32TypeInContext(context_), static_cast<std::uint64_t>(0x7F04), 0);
                    LLVMValueRef trapArguments[] = {trapReason};
                    buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
                    LLVMBuildUnreachable(builder_);
                }
                enterBlock(frameOk);
                for (const auto& [localOrdinal, offset, typeKey] : frameLocals)
                {
                    const auto localFound = localStorage_.find(localOrdinal);
                    if (localFound == localStorage_.end() || localFound->second == nullptr) continue;
                    LLVMTypeRef localType = llvmType(typeKey, error);
                    if (localType == nullptr) return nullptr;
                    LLVMBuildStore(
                        builder_,
                        LLVMBuildLoad2(builder_, localType, localFound->second, "ayield.local"),
                        byteOffset(frame, offset, "ayield.frame.local.slot"));
                }
                return frame;
}

LLVMValueRef OopLlvmImplementation::lowerAsyncYieldItem(
        const frontend::ProjectOopMirNode* node, LLVMValueRef frame, std::string& error)
{
                const auto& frameLocals = currentMirFunction_ != nullptr
                    ? currentMirFunction_->asyncFrameLocals
                    : std::vector<std::tuple<std::size_t, std::uint64_t, std::string>>{};
                LLVMValueRef item = nullptr;
                if (!node->children.empty())
                {
                    const bool itemByReference =
                        node->children.front()->kind == frontend::ProjectOopMirNodeKind::LocalAddress ||
                        node->children.front()->kind == frontend::ProjectOopMirNodeKind::FieldAddress;
                    if (itemByReference && node->children.front()->localOrdinal.has_value())
                    {
                        const auto* frameLocal = currentMirFunction_ == nullptr
                            ? nullptr
                            : mirModel_.asyncFrameLocal(
                                *currentMirFunction_, *node->children.front()->localOrdinal);
                        if (frameLocal == nullptr)
                            return failValue(error, "async ref item local has no frame slot");
                        item = byteOffset(frame, std::get<1>(*frameLocal), "ayield.item.frame.slot");
                    }
                    else
                    {
                        item = lowerExpression(node->children.front().get(), error);
                        if (item == nullptr) return nullptr;
                    }
                }
                return item;
}

bool OopLlvmImplementation::restoreAsyncYieldFrame(
        LLVMValueRef asyncIterator, std::string& error)
{
                const auto& frameLocals = currentMirFunction_ != nullptr
                    ? currentMirFunction_->asyncFrameLocals
                    : std::vector<std::tuple<std::size_t, std::uint64_t, std::string>>{};
                LLVMValueRef resumeFrame = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(asyncIterator, 8, "ayield.resume.frame.addr"), "ayield.resume.frame");
                for (const auto& [localOrdinal, offset, typeKey] : frameLocals)
                {
                    const auto localFound = localStorage_.find(localOrdinal);
                    if (localFound == localStorage_.end() || localFound->second == nullptr) continue;
                    LLVMTypeRef localType = llvmType(typeKey, error);
                    if (localType == nullptr) return false;
                    LLVMBuildStore(
                        builder_,
                        LLVMBuildLoad2(
                            builder_, localType, byteOffset(resumeFrame, offset, "ayield.frame.local.slot"),
                            "ayield.saved"),
                        localFound->second);
                }
                return true;
}

bool OopLlvmImplementation::lowerDropFrameStatement(
        [[maybe_unused]] const frontend::ProjectOopMirNode* node, std::string& error)
{
                // NS304-T06-F07: `Gen$drop` destroys the frame's live locals (reverse order) via
                // the drop glue.
                if (parameterStorage_.empty())
                    return fail(error, "drop frame has no self parameter");
                LLVMValueRef self = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0],
                    "drop.self");
                LLVMValueRef frame = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(self, 8, "drop.frame.addr"), "drop.frame");
                LLVMBasicBlockRef noFrame = appendBlock("drop.no.frame");
                LLVMBasicBlockRef doDrop = appendBlock("drop.do");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(
                        builder_, LLVMIntEQ, frame,
                        LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), "drop.frame.null"),
                    noFrame, doDrop);
                enterBlock(doDrop);
                const auto& frameLocals = currentMirFunction_ != nullptr
                    ? currentMirFunction_->asyncFrameLocals
                    : std::vector<std::tuple<std::size_t, std::uint64_t, std::string>>{};
                for (std::size_t index = frameLocals.size(); index > 0; --index)
                {
                    const auto& [localOrdinal, offset, typeKey] = frameLocals[index - 1];
                    const auto glue = valueDropEntries_.find(typeKey);
                    if (glue == valueDropEntries_.end()) continue;
                    LLVMValueRef args[] = { byteOffset(frame, offset, "drop.slot") };
                    if (LLVMBuildCall2(
                            builder_, glue->second.functionType, glue->second.function,
                            args, 1U, "") == nullptr)
                        return fail(error, "drop glue call failed");
                }
                // NS304-T07-F04: the async-iterator's frame is heap (allocated by the constructor's
                // GeneratorFrameInit). Release it exactly once and null it + park the terminal state
                // so a second destruction never re-drops or re-frees. The sync iterator is left alone
                // here: its foreach teardown still owns the frame twice (T06-F08 double-owner), so a
                // shared free would double-free; that repair is a separate causal commit.
                const bool asyncIterator = currentMirFunction_ != nullptr &&
                    !currentMirFunction_->parameterTypeKeys.empty() &&
                    currentMirFunction_->parameterTypeKeys[0].starts_with("async-iterator:");
                if (asyncIterator)
                {
                    if (deallocationFunction_ == nullptr || deallocationFunctionType_ == nullptr)
                        return fail(error, "async iterator drop deallocation is unavailable");
                    LLVMValueRef freeArgs[] = { frame };
                    buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, freeArgs, 1, "");
                    LLVMBuildStore(
                        builder_, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)),
                        byteOffset(self, 8, "drop.frame.null.store"));
                    LLVMTypeRef dropStateType = llvmType("builtin:int", error);
                    if (dropStateType == nullptr) return false;
                    LLVMBuildStore(
                        builder_, LLVMConstInt(dropStateType, static_cast<unsigned long long>(-1), 1),
                        self);
                }
                LLVMBuildBr(builder_, noFrame);
                enterBlock(noFrame);
                return true;
}

bool OopLlvmImplementation::emitDefaultLocalStorage(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || !node->localOrdinal.has_value())
        {
            return true;
        }
        const auto slot = localStorage_.find(*node->localOrdinal);
        if (slot == localStorage_.end() || slot->second == nullptr)
        {
            return fail(error, "Stage 56 C7 default local storage has no alloca");
        }
        LLVMTypeRef type = llvmType(node->typeKey, error);
        if (type == nullptr)
        {
            return false;
        }
        // LLVMConstNull covers every shape this can be -- integers and floats zero, pointers
        // null, and an indirect-aggregate carrier `[N x i8]` becomes `zeroinitializer`, which the
        // backend expands into the memset it would have taken to write by hand.
        LLVMValueRef store = LLVMBuildStore(builder_, LLVMConstNull(type), slot->second);
        const unsigned structAlignment = structAlignmentFor(node->typeKey);
        if (structAlignment > 1U)
        {
            LLVMSetAlignment(store, structAlignment);
        }
        return true;
    }

}
