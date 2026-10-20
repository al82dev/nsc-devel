#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

std::optional<StaticLocalGuard> OopLlvmImplementation::beginStaticLocalInitialization(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        StaticLocalStorage* storage = findStaticLocalStorage(node, error);
        if (storage == nullptr) return std::nullopt;

        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMValueRef zero = LLVMConstInt(i8, 0, 0);
        LLVMValueRef one = LLVMConstInt(i8, 1, 0);
        LLVMValueRef two = LLVMConstInt(i8, 2, 0);
        LLVMValueRef three = LLVMConstInt(i8, 3, 0);
        LLVMBasicBlockRef fastFailure = appendBlock("static.local.fast_failure");
        LLVMBasicBlockRef casAttempt = appendBlock("static.local.cas");
        LLVMBasicBlockRef casTerminal = appendBlock("static.local.cas_terminal");
        LLVMBasicBlockRef recursion = appendBlock("static.local.recursion");
        LLVMBasicBlockRef spin = appendBlock("static.local.spin");
        LLVMBasicBlockRef spinTerminal = appendBlock("static.local.spin_terminal");
        LLVMBasicBlockRef initialize = appendBlock("static.local.initialize");
        LLVMBasicBlockRef done = appendBlock("static.local.done");
        LLVMBasicBlockRef failed = appendBlock("static.local.failed");
        LLVMBasicBlockRef failureUnwind = appendBlock("static.local.failure_unwind");

        emitStaticLocalFastState(
            *storage, i8, two, three, done, fastFailure, failed, casAttempt);

        LLVMPositionBuilderAtEnd(builder_, casAttempt);
        LLVMValueRef threadId = buildNoUnwindCall(
            getCurrentThreadIdFunctionType_, getCurrentThreadIdFunction_, nullptr, 0,
            "static.local.current_tid");
        LLVMValueRef cas = LLVMBuildAtomicCmpXchg(
            builder_, storage->state, zero, one,
            LLVMAtomicOrderingSequentiallyConsistent,
            LLVMAtomicOrderingSequentiallyConsistent, 0);
        LLVMValueRef observed = LLVMBuildExtractValue(builder_, cas, 0, "static.local.observed");
        LLVMValueRef won = LLVMBuildExtractValue(builder_, cas, 1, "static.local.won");
        LLVMBuildCondBr(builder_, won, initialize, casTerminal);

        LLVMPositionBuilderAtEnd(builder_, casTerminal);
        LLVMValueRef observedDone = LLVMBuildICmp(builder_, LLVMIntEQ, observed, two, "static.local.cas_done");
        LLVMBasicBlockRef casFailure = appendBlock("static.local.cas_failure");
        LLVMBuildCondBr(builder_, observedDone, done, casFailure);
        LLVMPositionBuilderAtEnd(builder_, casFailure);
        LLVMValueRef observedFailed = LLVMBuildICmp(builder_, LLVMIntEQ, observed, three, "static.local.cas_failed");
        LLVMBuildCondBr(builder_, observedFailed, failed, recursion);

        LLVMPositionBuilderAtEnd(builder_, recursion);
        LLVMValueRef owner = LLVMBuildLoad2(builder_, i32, storage->owner, "static.local.owner_tid");
        LLVMSetOrdering(owner, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMValueRef sameThread = LLVMBuildICmp(builder_, LLVMIntEQ, owner, threadId, "static.local.is_self");
        LLVMBuildCondBr(builder_, sameThread, done, spin);

        LLVMPositionBuilderAtEnd(builder_, spin);
        LLVMValueRef spinState = LLVMBuildLoad2(builder_, i8, storage->state, "static.local.state.spin");
        LLVMSetOrdering(spinState, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMValueRef pending = LLVMBuildICmp(builder_, LLVMIntEQ, spinState, one, "static.local.pending");
        LLVMBuildCondBr(builder_, pending, spin, spinTerminal);
        LLVMPositionBuilderAtEnd(builder_, spinTerminal);
        LLVMValueRef spinDone = LLVMBuildICmp(builder_, LLVMIntEQ, spinState, two, "static.local.spin_done");
        LLVMBuildCondBr(builder_, spinDone, done, failed);

        LLVMPositionBuilderAtEnd(builder_, initialize);
        LLVMValueRef ownerStore = LLVMBuildStore(builder_, threadId, storage->owner);
        LLVMSetOrdering(ownerStore, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
        staticInitializationFailureUnwindBlock_ = failureUnwind;
        return StaticLocalGuard{storage->state, done, failed, failureUnwind};
    }

StaticLocalStorage* OopLlvmImplementation::findStaticLocalStorage(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node == nullptr || !node->localOrdinal.has_value())
        {
            fail(error, "T01-F06 static local guard lost declaration identity");
            return nullptr;
        }
        const auto found = staticLocalStorage_.find(*node->localOrdinal);
        if (found == staticLocalStorage_.end())
        {
            fail(error, "T01-F06 static local guard cannot find module storage");
            return nullptr;
        }
        return &found->second;
    }

void OopLlvmImplementation::emitStaticLocalFastState(
        const StaticLocalStorage& storage,
        LLVMTypeRef i8,
        LLVMValueRef two,
        LLVMValueRef three,
        LLVMBasicBlockRef done,
        LLVMBasicBlockRef fastFailure,
        LLVMBasicBlockRef failed,
        LLVMBasicBlockRef casAttempt)
{
        LLVMValueRef state = LLVMBuildLoad2(builder_, i8, storage.state, "static.local.state.fast");
        LLVMSetOrdering(state, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMValueRef initialized = LLVMBuildICmp(builder_, LLVMIntEQ, state, two, "static.local.fast_done");
        LLVMBuildCondBr(builder_, initialized, done, fastFailure);

        LLVMPositionBuilderAtEnd(builder_, fastFailure);
        LLVMValueRef isFailed = LLVMBuildICmp(builder_, LLVMIntEQ, state, three, "static.local.fast_failed");
        LLVMBuildCondBr(builder_, isFailed, failed, casAttempt);
    }

bool OopLlvmImplementation::finishStaticLocalInitialization(const StaticLocalGuard& guard, std::string& error)
{
        staticInitializationFailureUnwindBlock_ = nullptr;
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        if (!terminated())
        {
            LLVMValueRef publish = LLVMBuildStore(builder_, LLVMConstInt(i8, 2, 0), guard.state);
            LLVMSetOrdering(publish, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMBuildBr(builder_, guard.done);
        }

        LLVMPositionBuilderAtEnd(builder_, guard.failed);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        if (mirModel_.panicProfile() == language::PanicProfile::Abort)
        {
            LLVMValueRef reason = LLVMConstInt(i32, static_cast<std::uint64_t>(LlvmTrapReason::Panic), 0);
            LLVMValueRef arguments[] = {reason};
            buildNoUnwindCall(trapFunctionType_, trapFunction_, arguments, 1, "");
        }
        else
        {
            LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
            LLVMValueRef arguments[] = {
                LLVMConstInt(i32, kNativeSharpPanicExceptionCode, 0),
                LLVMConstInt(i32, 1, 0), LLVMConstInt(i32, 0, 0), LLVMConstNull(ptr)};
            buildNoUnwindCall(raiseExceptionFunctionType_, raiseExceptionFunction_, arguments, 4, "");
        }
        LLVMBuildUnreachable(builder_);

        LLVMPositionBuilderAtEnd(builder_, guard.failureUnwind);
        LLVMValueRef pad = LLVMBuildCleanupPad(builder_, nullptr, nullptr, 0, "static.local.failure.pad");
        if (pad == nullptr) return fail(error, "T01-F06 failed to create static-local failure cleanuppad");
        LLVMValueRef failed = LLVMBuildStore(builder_, LLVMConstInt(i8, 3, 0), guard.state);
        LLVMSetOrdering(failed, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMBuildCleanupRet(builder_, pad, nullptr);
        LLVMPositionBuilderAtEnd(builder_, guard.done);
        return true;
    }

bool OopLlvmImplementation::emitStaticLocalDestruction(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error)
{
        if (node == nullptr || storage == nullptr)
            return fail(error, "T01-F06 static-local destructor lost its declaration storage");
        if (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(node->typeKey))
            return emitValueDropAddress(storage, node->typeKey, "static.local.value", error);
        if (!node->automaticDestruction) return true;

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, storage, "static.local.owner");
        if (frontend::ProjectTypeConversion::isNativeArray(node->typeKey))
        {
            return emitNativeArrayDestroy(
                value, node->typeKey, "static.local.array",
                node->ownership != sema::OwnershipKind::Borrowed, error);
        }
        return emitOwnedPointerDestroy(value, node->typeKey, "static.local.owner", error);
    }

bool OopLlvmImplementation::defineStaticLocalDestructors(std::string& error)
{
        std::vector<std::size_t> ordinals;
        for (const auto& [ordinal, item] : staticLocalStorage_)
        {
            if (item.declaration != nullptr && item.declaration->automaticDestruction)
                ordinals.push_back(ordinal);
        }
        std::ranges::sort(ordinals);
        if (ordinals.empty()) return true;

        LLVMTypeRef voidType = LLVMVoidTypeInContext(context_);
        LLVMTypeRef functionType = LLVMFunctionType(voidType, nullptr, 0, 0);
        std::vector<LLVMValueRef> destructors;
        destructors.reserve(ordinals.size());
        for (const std::size_t ordinal : ordinals)
        {
            StaticLocalStorage& item = staticLocalStorage_.at(ordinal);
            const std::string name = "__nsc_static_local_destroy_" + std::to_string(ordinal);
            LLVMValueRef function = LLVMAddFunction(module_, name.c_str(), functionType);
            LLVMSetLinkage(function, LLVMPrivateLinkage);
            applyUwtableAttribute(function);
            currentFunction_ = function;
            currentMirFunction_ = nullptr;
            blockCounter_ = 0;
            localStorage_.clear();
            activeCleanupRegions_.clear();
            cleanupRegions_.clear();

            LLVMBasicBlockRef entry = appendBlock("static.local.destroy.entry");
            LLVMBasicBlockRef destroy = appendBlock("static.local.destroy.value");
            LLVMBasicBlockRef done = appendBlock("static.local.destroy.done");
            LLVMPositionBuilderAtEnd(builder_, entry);
            LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
            LLVMValueRef state = LLVMBuildLoad2(builder_, i8, item.state, "static.local.destroy.state");
            LLVMSetOrdering(state, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMValueRef initialized = LLVMBuildICmp(
                builder_, LLVMIntEQ, state, LLVMConstInt(i8, 2, 0), "static.local.destroy.live");
            LLVMBuildCondBr(builder_, initialized, destroy, done);

            LLVMPositionBuilderAtEnd(builder_, destroy);
            LLVMValueRef claim = LLVMBuildStore(builder_, LLVMConstInt(i8, 3, 0), item.state);
            LLVMSetOrdering(claim, LLVMAtomicOrderingSequentiallyConsistent);
            if (!emitStaticLocalDestruction(item.declaration, item.storage, error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, done);
            LLVMPositionBuilderAtEnd(builder_, done);
            LLVMBuildRetVoid(builder_);
            destructors.push_back(function);
        }

        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef fields[] = {i32, ptr, ptr};
        LLVMTypeRef entryType = LLVMStructTypeInContext(context_, fields, 3, 0);
        std::vector<LLVMValueRef> entries;
        entries.reserve(destructors.size());
        for (LLVMValueRef destructor : destructors)
        {
            LLVMValueRef values[] = {
                LLVMConstInt(i32, 65535, 0), destructor, LLVMConstNull(ptr)};
            entries.push_back(LLVMConstNamedStruct(entryType, values, 3));
        }
        LLVMTypeRef arrayType = LLVMArrayType2(entryType, entries.size());
        LLVMValueRef table = LLVMAddGlobal(module_, arrayType, "llvm.global_dtors");
        LLVMSetLinkage(table, LLVMAppendingLinkage);
        LLVMSetInitializer(table, LLVMConstArray2(entryType, entries.data(), entries.size()));
        currentFunction_ = nullptr;
        return true;
    }

}
