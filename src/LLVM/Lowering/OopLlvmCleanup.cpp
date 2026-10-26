#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

void OopLlvmImplementation::emitCleanupGuardBranch(
        LLVMValueRef ownerStorage,
        LLVMValueRef ownerValue,
        LLVMBasicBlockRef destroyBlock,
        LLVMBasicBlockRef doneBlock,
        const char* namePrefix)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef skip = LLVMBuildICmp(
            builder_, LLVMIntEQ, ownerValue, LLVMConstNull(ptr),
            (std::string(namePrefix) + ".isnull").c_str());
        LLVMBuildCondBr(builder_, skip, doneBlock, destroyBlock);

        enterBlock(destroyBlock);
        // Cleared BEFORE the destructor runs: that is what makes a cleanup entered twice destroy
        // only once, and it keeps the slot from holding a dangling pointer afterwards on any path
        // that legitimately survives the cleanup.
        LLVMBuildStore(builder_, LLVMConstNull(ptr), ownerStorage);
    }

std::optional<std::size_t> OopLlvmImplementation::activeCleanupIndex(
        std::size_t ordinal,
        const std::vector<std::size_t>& active) const noexcept
{
        const auto found = std::find(active.begin(), active.end(), ordinal);
        if (found == active.end()) return std::nullopt;
        return static_cast<std::size_t>(found - active.begin());
    }

LLVMValueRef OopLlvmImplementation::activeFuncletPad() const noexcept
{
        return funcletStack_.empty() ? nullptr : funcletStack_.back().pad;
    }

LLVMValueRef OopLlvmImplementation::buildNoUnwindCall(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        const char* name)
{
        LLVMValueRef pad = activeFuncletPad();
        if (pad == nullptr)
        {
            return LLVMBuildCall2(
                builder_, functionType, function, arguments, argumentCount, name);
        }
        LLVMValueRef bundleArguments[] = {pad};
        LLVMOperandBundleRef bundle = LLVMCreateOperandBundle(
            "funclet", 7, bundleArguments, 1);
        LLVMOperandBundleRef bundles[] = {bundle};
        LLVMValueRef call = LLVMBuildCallWithOperandBundles(
            builder_, functionType, function, arguments, argumentCount,
            bundles, 1, name);
        LLVMDisposeOperandBundle(bundle);
        return call;
    }

LLVMValueRef OopLlvmImplementation::buildInvokeInCurrentFunclet(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        LLVMBasicBlockRef normalBlock,
        LLVMBasicBlockRef unwindBlock,
        const char* name)
{
        LLVMValueRef pad = activeFuncletPad();
        if (pad == nullptr)
        {
            return LLVMBuildInvoke2(
                builder_, functionType, function, arguments, argumentCount,
                normalBlock, unwindBlock, name);
        }
        LLVMValueRef bundleArguments[] = {pad};
        LLVMOperandBundleRef bundle = LLVMCreateOperandBundle(
            "funclet", 7, bundleArguments, 1);
        LLVMOperandBundleRef bundles[] = {bundle};
        LLVMValueRef invoke = LLVMBuildInvokeWithOperandBundles(
            builder_, functionType, function, arguments, argumentCount,
            normalBlock, unwindBlock, bundles, 1, name);
        LLVMDisposeOperandBundle(bundle);
        return invoke;
    }

LLVMBasicBlockRef OopLlvmImplementation::emitExceptionalCleanupChain(
        const std::vector<std::size_t>& cleanupPath,
        LLVMBasicBlockRef finalUnwindTarget,
        std::string& error)
{
        if (cleanupPath.empty()) return finalUnwindTarget;
        LLVMBasicBlockRef sourceBlock = LLVMGetInsertBlock(builder_);
        if (sourceBlock == nullptr)
        {
            error = "Stage 45.10 cannot emit an exceptional cleanup chain without an insertion block";
            return nullptr;
        }

        const std::vector<std::size_t> savedActive = activeCleanupRegions_;
        const std::vector<FuncletContext> savedFunclets = funcletStack_;

        // Pass 1: resolve each ordinal's active-region index and funclet parent. Both are
        // properties of the CALL SITE that started this chain (fixed for the whole chain, the
        // same for every ordinal in it) -- nothing here depends on blocks built later in pass 2,
        // which is exactly what the old single-pass version got wrong: it computed `parentPad`
        // by scanning `savedFunclets` (the state from BEFORE this chain started) even for
        // ordinals later in the path, so an outer ordinal's cleanuppad never actually saw the
        // cleanuppad this same call just built for an inner ordinal as its parent.
        std::vector<std::size_t> regionIndices(cleanupPath.size());
        std::vector<LLVMValueRef> parentPads(cleanupPath.size());
        std::vector<std::size_t> parentPrefixes(cleanupPath.size());
        if (!resolveExceptionalCleanupPath(
                cleanupPath, savedActive, savedFunclets, sourceBlock,
                regionIndices, parentPads, parentPrefixes, error)) return nullptr;

        // Pass 2: build/reuse blocks from OUTERMOST to INNERMOST, so each ordinal's unwind
        // target ("next") is already resolved -- either a previously-cached cleanuppad or a
        // freshly built one -- by the time it is needed. Reusing a cached ordinal here is what
        // gives a cleanup region reached by several unwind chains (a RAII local's own scope-exit,
        // reached both from a try/catch's "no handler matched" edge and, independently, from a
        // sibling try/finally further down the same method) exactly ONE cleanuppad instead of
        // one independent "within none" cleanuppad per chain -- see CachedCleanup's comment and
        // BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md for the crash the duplicate
        // lowering used to produce.
        LLVMBasicBlockRef next = finalUnwindTarget;
        for (std::size_t reverseIndex = 0; reverseIndex < cleanupPath.size(); ++reverseIndex)
        {
            const std::size_t pathIndex = cleanupPath.size() - 1 - reverseIndex;
            const std::size_t ordinal = cleanupPath[pathIndex];
            const std::size_t regionIndex = regionIndices[pathIndex];
            const LLVMValueRef parentPad = parentPads[pathIndex];
            const std::size_t parentPrefix = parentPrefixes[pathIndex];

            const auto cached = cleanupChainCache_.find(ordinal);
            if (cached != cleanupChainCache_.end() && cached->second.parentPad == parentPad &&
                cached->second.next == next)
            {
                next = cached->second.block;
                continue;
            }

            LLVMBasicBlockRef block = appendBlock("eh.cleanup." + std::to_string(ordinal));
            funcletStack_.clear();
            for (std::size_t index = 0; index < parentPrefix; ++index)
            {
                funcletStack_.push_back(savedFunclets[index]);
            }
            activeCleanupRegions_.clear();
            for (std::size_t index = 0; index < regionIndex; ++index)
            {
                activeCleanupRegions_.push_back(savedActive[index]);
            }
            enterBlock(block);
            LLVMValueRef cleanupPad = LLVMBuildCleanupPad(
                builder_, parentPad, nullptr, 0, "eh.cleanuppad");
            if (cleanupPad == nullptr)
            {
                activeCleanupRegions_ = savedActive;
                funcletStack_ = savedFunclets;
                enterBlock(sourceBlock);
                error = "Stage 45.10 failed to create cleanuppad";
                return nullptr;
            }
            funcletStack_.push_back(FuncletContext{
                cleanupPad, FuncletKind::Cleanup, regionIndex, nullptr, next});

            if (!emitCleanupAction(ordinal, error))
            {
                activeCleanupRegions_ = savedActive;
                funcletStack_ = savedFunclets;
                enterBlock(sourceBlock);
                return nullptr;
            }
            if (!terminated())
            {
                LLVMBuildCleanupRet(builder_, cleanupPad, next);
            }

            cleanupChainCache_[ordinal] = CachedCleanup{parentPad, next, block};
            next = block;
        }

        activeCleanupRegions_ = savedActive;
        funcletStack_ = savedFunclets;
        enterBlock(sourceBlock);
        return next;
    }

bool OopLlvmImplementation::resolveExceptionalCleanupPath(
        const std::vector<std::size_t>& cleanupPath,
        const std::vector<std::size_t>& savedActive,
        const std::vector<FuncletContext>& savedFunclets,
        LLVMBasicBlockRef sourceBlock,
        std::vector<std::size_t>& regionIndices,
        std::vector<LLVMValueRef>& parentPads,
        std::vector<std::size_t>& parentPrefixes,
        std::string& error)
{
        for (std::size_t pathIndex = 0; pathIndex < cleanupPath.size(); ++pathIndex)
        {
            const std::size_t ordinal = cleanupPath[pathIndex];
            const auto cleanupFound = cleanupRegions_.find(ordinal);
            const std::optional<std::size_t> regionIndex = activeCleanupIndex(ordinal, savedActive);
            if (cleanupFound == cleanupRegions_.end() || cleanupFound->second == nullptr ||
                !regionIndex.has_value())
            {
                enterBlock(sourceBlock);
                error = "Stage 45.10 exceptional cleanup path references an inactive MIR cleanup region";
                return false;
            }
            regionIndices[pathIndex] = *regionIndex;

            LLVMValueRef parentPad = nullptr;
            std::size_t parentPrefix = 0;
            for (std::size_t index = 0; index < savedFunclets.size(); ++index)
            {
                if (*regionIndex >= savedFunclets[index].cleanupDepthAtEntry)
                {
                    parentPad = savedFunclets[index].pad;
                    parentPrefix = index + 1;
                }
            }
            parentPads[pathIndex] = parentPad;
            parentPrefixes[pathIndex] = parentPrefix;
        }
        return true;
    }

LLVMBasicBlockRef OopLlvmImplementation::buildCurrentUnwindDestination(std::string& error)
{
        std::size_t retainedDepth = 0;
        LLVMBasicBlockRef finalTarget = nullptr;

        const bool hasLocalHandler = !exceptionHandlerStack_.empty() &&
            exceptionHandlerStack_.back().funcletDepthAtEntry == funcletStack_.size();
        if (hasLocalHandler)
        {
            retainedDepth = std::min(
                exceptionHandlerStack_.back().retainedCleanupDepth,
                activeCleanupRegions_.size());
            finalTarget = exceptionHandlerStack_.back().dispatchBlock;
        }
        else if (!funcletStack_.empty())
        {
            retainedDepth = std::min(
                funcletStack_.back().cleanupDepthAtEntry,
                activeCleanupRegions_.size());
            finalTarget = funcletStack_.back().unwindDestination;
        }
        else if (staticInitializationFailureUnwindBlock_ != nullptr)
        {
            finalTarget = staticInitializationFailureUnwindBlock_;
        }

        // A captured local/parameter has one reference owned by the creating activation. If an
        // invoke leaves that activation, normal return lowering never runs, so put that reference
        // behind a function-level SEH cleanup. Local catches deliberately retain the cells: the
        // method is still alive and may continue using both the local and closures after catch.
        if (!hasLocalHandler && finalTarget == nullptr && !ownedCaptureCells_.empty())
        {
            finalTarget = captureCellExceptionalCleanup(error);
            if (finalTarget == nullptr) return nullptr;
        }

        std::vector<std::size_t> path;
        for (std::size_t index = activeCleanupRegions_.size(); index > retainedDepth; --index)
        {
            path.push_back(activeCleanupRegions_[index - 1]);
        }
        return path.empty()
            ? finalTarget
            : emitExceptionalCleanupChain(path, finalTarget, error);
    }

LLVMValueRef OopLlvmImplementation::buildPotentiallyThrowingCall(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        const char* name,
        std::string& error)
{
        LLVMBasicBlockRef unwindBlock = buildCurrentUnwindDestination(error);
        if (!error.empty()) return nullptr;
        if (unwindBlock == nullptr)
        {
            return buildNoUnwindCall(
                functionType, function, arguments, argumentCount, name);
        }
        LLVMBasicBlockRef normalBlock = appendBlock("eh.invoke.cont");
        LLVMValueRef invoke = buildInvokeInCurrentFunclet(
            functionType, function, arguments, argumentCount,
            normalBlock, unwindBlock, name);
        if (invoke == nullptr)
        {
            error = "Stage 45.10 failed to emit invoke for a potentially throwing call";
            return nullptr;
        }
        enterBlock(normalBlock);
        return invoke;
    }

bool OopLlvmImplementation::findInterfaceCleanupTarget(
        std::size_t contractMemberOrdinal,
        std::uint64_t& stableInterfaceId,
        std::size_t& slot) const noexcept
{
        return mirModel_.interfaceSlotForMember(
            contractMemberOrdinal, stableInterfaceId, slot);
    }

bool OopLlvmImplementation::emitInterfaceDispose(
        LLVMValueRef receiver,
        std::size_t contractMemberOrdinal,
        std::string& error)
{
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr ||
            interfaceMapEntryType_ == nullptr)
        {
            return fail(error, "Stage 45.10 interface Dispose has no interface-map ABI metadata");
        }
        std::uint64_t interfaceId = 0;
        std::size_t slot = 0;
        if (!findInterfaceCleanupTarget(contractMemberOrdinal, interfaceId, slot))
        {
            return fail(error, "Stage 45.10 cannot resolve IDisposable.Dispose interface slot");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "dispose.interface.descriptor");
        LLVMValueRef mapAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 4, "dispose.interface.map.addr");
        LLVMValueRef map = LLVMBuildLoad2(builder_, ptr, mapAddress, "dispose.interface.map");
        return emitInterfaceDisposeSearch(receiver, map, interfaceId, slot, error);
    }

bool OopLlvmImplementation::emitInterfaceDisposeSearch(
        LLVMValueRef receiver,
        LLVMValueRef map,
        std::uint64_t interfaceId,
        std::size_t slot,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMBasicBlockRef mapBlock = appendBlock("dispose.interface.map.ok");
        LLVMBasicBlockRef searchBlock = appendBlock("dispose.interface.search");
        LLVMBasicBlockRef probeBlock = appendBlock("dispose.interface.probe");
        LLVMBasicBlockRef nextBlock = appendBlock("dispose.interface.next");
        LLVMBasicBlockRef foundBlock = appendBlock("dispose.interface.found");
        LLVMBasicBlockRef missingBlock = appendBlock("dispose.interface.missing");
        LLVMBasicBlockRef doneBlock = appendBlock("dispose.interface.done");

        LLVMValueRef hasMap = LLVMBuildICmp(
            builder_, LLVMIntNE, map, LLVMConstNull(ptr), "dispose.interface.map.nonnull");
        LLVMBuildCondBr(builder_, hasMap, mapBlock, missingBlock);

        enterBlock(mapBlock);
        LLVMValueRef countAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 0, "dispose.interface.map.count.addr");
        LLVMValueRef count = LLVMBuildLoad2(builder_, i64, countAddress, "dispose.interface.map.count");
        LLVMValueRef entriesAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 1, "dispose.interface.map.entries.addr");
        LLVMValueRef entries = LLVMBuildLoad2(builder_, ptr, entriesAddress, "dispose.interface.map.entries");
        LLVMBuildBr(builder_, searchBlock);

        enterBlock(searchBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "dispose.interface.map.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {mapBlock};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef inRange = LLVMBuildICmp(
            builder_, LLVMIntULT, index, count, "dispose.interface.map.inrange");
        LLVMBuildCondBr(builder_, inRange, probeBlock, missingBlock);

        enterBlock(probeBlock);
        LLVMValueRef entryIndices[] = {index};
        LLVMValueRef entry = LLVMBuildGEP2(
            builder_, interfaceMapEntryType_, entries, entryIndices, 1, "dispose.interface.entry");
        LLVMValueRef idAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 0, "dispose.interface.entry.id.addr");
        LLVMValueRef id = LLVMBuildLoad2(builder_, i64, idAddress, "dispose.interface.entry.id");
        LLVMValueRef wanted = LLVMConstInt(i64, interfaceId, 0);
        LLVMValueRef matches = LLVMBuildICmp(
            builder_, LLVMIntEQ, id, wanted, "dispose.interface.entry.matches");
        LLVMBuildCondBr(builder_, matches, foundBlock, nextBlock);

        enterBlock(nextBlock);
        LLVMValueRef one = LLVMConstInt(i64, 1, 0);
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, index, one, "dispose.interface.map.next");
        LLVMBuildBr(builder_, searchBlock);
        LLVMValueRef nextValues[] = {nextIndex};
        LLVMBasicBlockRef nextBlocks[] = {nextBlock};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        enterBlock(missingBlock);
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::InterfaceDispatchFailure), 0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        enterBlock(foundBlock);
        return emitInterfaceDisposeTarget(receiver, entry, slot, doneBlock, error);
    }

bool OopLlvmImplementation::emitInterfaceDisposeTarget(
        LLVMValueRef receiver,
        LLVMValueRef entry,
        std::size_t slot,
        LLVMBasicBlockRef doneBlock,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef tableAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 1, "dispose.interface.entry.table.addr");
        LLVMValueRef methodTable = LLVMBuildLoad2(builder_, ptr, tableAddress, "dispose.interface.method.table");
        LLVMValueRef slotIndex = LLVMConstInt(i64, slot, 0);
        LLVMValueRef slotIndices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, methodTable, slotIndices, 1, "dispose.interface.slot.addr");
        LLVMValueRef target = LLVMBuildLoad2(builder_, ptr, slotAddress, "dispose.interface.target");
        LLVMTypeRef parameterTypes[] = {ptr};
        LLVMTypeRef disposeType = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameterTypes, 1, 0);
        LLVMValueRef arguments[] = {receiver};
        if (buildPotentiallyThrowingCall(
                disposeType, target, arguments, 1, "", error) == nullptr && !error.empty())
        {
            return false;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

bool OopLlvmImplementation::emitUsingDispose(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || node->kind != frontend::ProjectOopMirNodeKind::UsingRegion ||
            !node->localOrdinal.has_value() || !node->disposeContractMemberOrdinal.has_value())
        {
            return fail(error, "Stage 45.10 using cleanup is missing frozen Dispose metadata");
        }
        const auto localFound = localStorage_.find(*node->localOrdinal);
        if (localFound == localStorage_.end() || localFound->second == nullptr)
        {
            return fail(error, "Stage 45.10 using cleanup cannot find its resource local");
        }
        LLVMTypeRef resourceType = llvmType(node->elementTypeKey, error);
        if (resourceType == nullptr) return false;
        LLVMValueRef resource = LLVMBuildLoad2(
            builder_, resourceType, localFound->second, "using.resource.cleanup");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, resource, LLVMConstNull(ptr), "using.resource.isnull");
        LLVMBasicBlockRef disposeBlock = appendBlock("using.dispose");
        LLVMBasicBlockRef doneBlock = appendBlock("using.dispose.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, disposeBlock);

        enterBlock(disposeBlock);
        if (node->interfaceCleanupDispatch)
        {
            if (!emitInterfaceDispose(resource, *node->disposeContractMemberOrdinal, error)) return false;
        }
        else
        {
            if (!node->disposeImplementationMemberOrdinal.has_value())
            {
                return fail(error, "Stage 45.10 direct using cleanup has no Dispose implementation");
            }
            const frontend::ProjectOopMirFunction* disposeFunction =
                mirModel_.functionForMember(*node->disposeImplementationMemberOrdinal);
            if (disposeFunction == nullptr)
            {
                return fail(error, "Stage 45.10 direct using cleanup cannot resolve Dispose MIR function");
            }
            const auto target = functions_.find(disposeFunction->mangledName);
            if (target == functions_.end())
            {
                return fail(error, "Stage 45.10 direct using cleanup cannot resolve Dispose LLVM function");
            }
            LLVMValueRef arguments[] = {resource};
            if (buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function,
                    arguments, 1, "", error) == nullptr && !error.empty())
            {
                return false;
            }
        }
        // EN: Freeze 3.0 C-01: an owning using local (initialized by `new`) is destroyed right
        //     after Dispose. The slot is cleared before the release entry runs, the same guard
        //     emitRaiiDestroy uses, so a cleanup entered twice disposes and destroys once.
        // ES: Congelamiento 3.0 C-01: un local de using dueno (inicializado con `new`) se destruye
        //     justo despues de Dispose. El slot se limpia antes de la entrada de liberacion, la
        //     misma guarda de emitRaiiDestroy, asi que un cleanup que entra dos veces lo hace una.
        if (node->automaticDestruction && !terminated())
        {
            if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
                return fail(error, "freeze 3.0 owning using cleanup has no descriptor ABI");
            LLVMBasicBlockRef releaseBlock = appendBlock("using.release");
            emitCleanupGuardBranch(
                localFound->second, resource, releaseBlock, doneBlock, "using.owner");
            LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, resource, "using.descriptor");
            LLVMValueRef entryAddress = LLVMBuildStructGEP2(
                builder_, descriptorType_, descriptor, 5U, "using.release.entry.addr");
            LLVMValueRef releaseEntry = LLVMBuildLoad2(
                builder_, ptr, entryAddress, "using.release.entry");
            LLVMValueRef releaseArguments[] = {resource};
            if (buildPotentiallyThrowingCall(
                    destructionEntryFunctionType_, releaseEntry, releaseArguments, 1, "", error) ==
                    nullptr && !error.empty())
            {
                return false;
            }
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

bool OopLlvmImplementation::emitNativeArrayDestroy(
        LLVMValueRef array,
        std::string_view arrayTypeKey,
        std::string_view stem,
        bool destroyElements,
        std::string& error)
{
        const std::optional<std::string> elementType =
            frontend::ProjectTypeConversion::nativeArrayElementType(arrayTypeKey);
        if (!elementType.has_value())
        {
            return fail(error, "NativeSharp 3.0 array destruction lost its element type");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        const std::string prefix(stem);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, array, LLVMConstNull(ptr), (prefix + ".isnull").c_str());
        LLVMBasicBlockRef destroyBlock = appendBlock(prefix + ".destroy");
        LLVMBasicBlockRef doneBlock = appendBlock(prefix + ".done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);
        enterBlock(destroyBlock);

        if (destroyElements && requiresOwnedDestruction(*elementType))
        {
            if (!emitNativeArrayElementDestruction(
                    array, arrayTypeKey, *elementType, prefix, destroyBlock, error)) return false;
        }
        else if (destroyElements && memberModel_ != nullptr &&
                 memberModel_->typeRequiresValueDrop(*elementType))
        {
            if (!emitNativeArrayValueElementDrop(array, arrayTypeKey, *elementType, prefix, error))
                return false;
        }

        LLVMValueRef arguments[] = {array};
        buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

// EN: Freeze 3.0 C-12: an owning array of structs with a destructor or owned fields destroyed only
//     its buffer, so every element's resources leaked. The elements are dropped in place, last
//     to first, before the buffer is released; a moved-out element was left zeroed by its move.
// ES: Congelamiento 3.0 C-12: un arreglo dueno de structs con destructor o campos owned solo
//     liberaba su buffer, asi que los recursos de cada elemento se fugaban. Los elementos se
//     destruyen en su lugar, del ultimo al primero, antes de liberar el buffer; un elemento movido
//     quedo en cero por su move.
bool OopLlvmImplementation::emitNativeArrayValueElementDrop(
        LLVMValueRef array,
        std::string_view arrayTypeKey,
        std::string_view elementType,
        const std::string& prefix,
        std::string& error)
{
        const auto [elementSize, elementAlignment] = captureValueLayout(elementType, error);
        if (!error.empty()) return false;
        if (elementSize == 0 || elementAlignment == 0)
            return fail(error, "Freeze 3.0 C-12 array element has no value layout");
        const std::size_t rank = frontend::ProjectTypeConversion::nativeArrayRank(arrayTypeKey);
        const std::uint64_t header = rank > 1 ? 8U * (1U + 2U * rank) : 8U;
        const std::uint64_t dataOffset = (header + elementAlignment - 1U) / elementAlignment * elementAlignment;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef length = LLVMBuildLoad2(builder_, i64, array, (prefix + ".values.length").c_str());
        LLVMBasicBlockRef entryBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef conditionBlock = appendBlock(prefix + ".values.cond");
        LLVMBasicBlockRef bodyBlock = appendBlock(prefix + ".values.body");
        LLVMBasicBlockRef doneBlock = appendBlock(prefix + ".values.done");
        LLVMBuildBr(builder_, conditionBlock);

        enterBlock(conditionBlock);
        LLVMValueRef remaining = LLVMBuildPhi(builder_, i64, (prefix + ".values.remaining").c_str());
        LLVMAddIncoming(remaining, &length, &entryBlock, 1);
        LLVMValueRef more = LLVMBuildICmp(
            builder_, LLVMIntNE, remaining, LLVMConstInt(i64, 0, 0), (prefix + ".values.more").c_str());
        LLVMBuildCondBr(builder_, more, bodyBlock, doneBlock);

        enterBlock(bodyBlock);
        LLVMValueRef index = LLVMBuildSub(
            builder_, remaining, LLVMConstInt(i64, 1, 0), (prefix + ".values.index").c_str());
        LLVMValueRef offset = LLVMBuildAdd(
            builder_,
            LLVMBuildMul(builder_, index, LLVMConstInt(i64, elementSize, 0), (prefix + ".values.scaled").c_str()),
            LLVMConstInt(i64, dataOffset, 0), (prefix + ".values.offset").c_str());
        LLVMValueRef indices[] = {offset};
        LLVMValueRef address = LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), array, indices, 1, (prefix + ".values.addr").c_str());
        if (!emitValueDropAddress(address, elementType, prefix + ".value", error)) return false;
        LLVMBasicBlockRef latch = LLVMGetInsertBlock(builder_);
        if (!terminated())
        {
            LLVMBuildBr(builder_, conditionBlock);
            LLVMAddIncoming(remaining, &index, &latch, 1);
        }
        enterBlock(doneBlock);
        return true;
    }

// EN: Freeze 3.0 C-12: `delete items[i]` of a struct element that needs a drop destroys the element
//     in place and zeroes its slot, so the slot holds the default value afterwards.
// ES: Congelamiento 3.0 C-12: `delete items[i]` de un elemento struct que requiere drop destruye el
//     elemento en su lugar y pone su slot en cero, asi el slot queda con el valor por defecto.
bool OopLlvmImplementation::emitValueElementDelete(
        LLVMValueRef address,
        std::string_view elementType,
        std::string& error)
{
        const auto [elementSize, elementAlignment] = captureValueLayout(elementType, error);
        if (!error.empty()) return false;
        if (elementSize == 0 || elementAlignment == 0 || memsetFunction_ == nullptr)
            return fail(error, "Freeze 3.0 C-12 deleted element has no value layout");
        if (!emitValueDropAddress(address, elementType, "delete.value", error)) return false;
        if (terminated()) return true;
        LLVMValueRef arguments[] = {
            address,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), elementSize, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, arguments, 4, "");
        return true;
    }

bool OopLlvmImplementation::emitNativeArrayElementDestruction(
        LLVMValueRef array,
        std::string_view arrayTypeKey,
        std::string_view elementType,
        const std::string& prefix,
        LLVMBasicBlockRef destroyBlock,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef length = LLVMBuildLoad2(builder_, i64, array, (prefix + ".length").c_str());
        LLVMBasicBlockRef conditionBlock = appendBlock(prefix + ".elements.cond");
        LLVMBasicBlockRef bodyBlock = appendBlock(prefix + ".elements.body");
        LLVMBasicBlockRef releaseBlock = appendBlock(prefix + ".release");
        LLVMBuildBr(builder_, conditionBlock);

        enterBlock(conditionBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, (prefix + ".index").c_str());
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMAddIncoming(index, &zero, &destroyBlock, 1);
        LLVMValueRef inRange = LLVMBuildICmp(
            builder_, LLVMIntULT, index, length, (prefix + ".inrange").c_str());
        LLVMBuildCondBr(builder_, inRange, bodyBlock, releaseBlock);

        enterBlock(bodyBlock);
        LLVMValueRef scaled = LLVMBuildMul(
            builder_, index, LLVMConstInt(i64, 8, 0), (prefix + ".scaled").c_str());
        const std::size_t rank = frontend::ProjectTypeConversion::nativeArrayRank(arrayTypeKey);
        const std::uint64_t elementDataOffset = rank > 1 ? 8U * (1U + 2U * rank) : 8U;
        LLVMValueRef offset = LLVMBuildAdd(
            builder_, scaled, LLVMConstInt(i64, elementDataOffset, 0),
            (prefix + ".offset").c_str());
        LLVMValueRef indices[] = {offset};
        LLVMValueRef slot = LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), array, indices, 1,
            (prefix + ".slot").c_str());
        LLVMValueRef child = LLVMBuildLoad2(
            builder_, ptr, slot, (prefix + ".element").c_str());
        // Clear before any user destructor can transfer control through EH.
        LLVMBuildStore(builder_, LLVMConstNull(ptr), slot);
        if (!emitOwnedPointerDestroy(child, elementType, prefix + ".element.destroy", error)) return false;
        LLVMBasicBlockRef incrementBlock = LLVMGetInsertBlock(builder_);
        if (!terminated())
        {
            LLVMValueRef next = LLVMBuildAdd(
                builder_, index, LLVMConstInt(i64, 1, 0), (prefix + ".next").c_str());
            LLVMBuildBr(builder_, conditionBlock);
            LLVMAddIncoming(index, &next, &incrementBlock, 1);
        }

        enterBlock(releaseBlock);
        return true;
    }

bool OopLlvmImplementation::emitOwnedPointerDestroy(
        LLVMValueRef value,
        std::string_view typeKey,
        std::string_view stem,
        std::string& error)
{
        if (frontend::ProjectTypeConversion::isNativeArray(typeKey))
            return emitNativeArrayDestroy(value, typeKey, stem, true, error);

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        const std::string prefix(stem);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, value, LLVMConstNull(ptr), (prefix + ".isnull").c_str());
        LLVMBasicBlockRef destroyBlock = appendBlock(prefix + ".destroy");
        LLVMBasicBlockRef doneBlock = appendBlock(prefix + ".done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);
        enterBlock(destroyBlock);

        if (isDelegateReferenceType(nameLookup_, typeKey))
        {
            if (!emitDelegateDestroy(value, prefix + ".delegate", error)) return false;
        }
        else if (typeKey == "builtin:string")
        {
            LLVMValueRef arguments[] = {value};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
        }
        else if (isClassReferenceType(nameLookup_, typeKey))
        {
            if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
                return fail(error, "NativeSharp 3.0 array element cleanup has no class destruction ABI");
            LLVMValueRef descriptor = LLVMBuildLoad2(
                builder_, ptr, value, (prefix + ".descriptor").c_str());
            LLVMValueRef entryAddress = LLVMBuildStructGEP2(
                builder_, descriptorType_, descriptor, 5, (prefix + ".entry.addr").c_str());
            LLVMValueRef destructionEntry = LLVMBuildLoad2(
                builder_, ptr, entryAddress, (prefix + ".entry").c_str());
            LLVMValueRef arguments[] = {value};
            if (buildPotentiallyThrowingCall(
                    destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
                !error.empty())
                return false;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

void OopLlvmImplementation::collectVariantCaseIndices(
        const frontend::ProjectOopMirStructLayout& layout,
        std::unordered_map<std::string, std::uint64_t>& caseIndices) const
{
        constexpr std::string_view factoryPrefix = "variant-case-factory:";
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(layout.typeOrdinal))
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const frontend::ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
            if (member.syntax == nullptr) continue;
            const std::string_view marker = member.syntax->auxiliaryText();
            if (!marker.starts_with(factoryPrefix)) continue;
            const std::size_t separator = marker.rfind(':');
            if (separator == std::string_view::npos || separator < factoryPrefix.size()) continue;
            std::uint64_t index = 0;
            bool valid = separator + 1U < marker.size();
            for (std::size_t position = separator + 1U; valid && position < marker.size(); ++position)
            {
                const char digit = marker[position];
                valid = digit >= '0' && digit <= '9';
                if (valid) index = index * 10U + static_cast<std::uint64_t>(digit - '0');
            }
            if (valid)
                caseIndices.emplace(
                    std::string(marker.substr(factoryPrefix.size(), separator - factoryPrefix.size())),
                    index);
        }
    }

bool OopLlvmImplementation::collectVariantDropCases(
        const frontend::ProjectOopMirStructLayout& payloadLayout,
        const std::unordered_map<std::string, std::uint64_t>& caseIndices,
        std::vector<DropCase>& dropCases,
        std::string& error) const
{
        constexpr std::string_view payloadPrefix = "variant-case-payload:";
        for (const frontend::ObjectFieldLayout& field : payloadLayout.fields)
        {
            if (field.memberOrdinal >= memberModel_->members().size()) continue;
            const frontend::ClassMemberSymbol& symbol = memberModel_->members()[field.memberOrdinal];
            if (symbol.syntax == nullptr) continue;
            const std::string_view marker = symbol.syntax->auxiliaryText();
            if (!marker.starts_with(payloadPrefix)) continue;
            const auto index = caseIndices.find(std::string(marker.substr(payloadPrefix.size())));
            if (index == caseIndices.end())
                return fail(error, "NativeSharp 3.0 variant drop cannot map payload to case tag");
            const bool owned = symbol.declaredOwnership == sema::OwnershipKind::Owned &&
                (requiresOwnedDestruction(field.typeKey) ||
                 frontend::ProjectTypeConversion::isNativeArray(field.typeKey));
            if (owned || memberModel_->typeRequiresValueDrop(field.typeKey))
                dropCases.push_back(DropCase{index->second, &field, &symbol});
        }
        return true;
    }

bool OopLlvmImplementation::emitVariantDropCases(
        LLVMValueRef address,
        const frontend::ObjectFieldLayout& payloadField,
        LLVMTypeRef tagType,
        LLVMValueRef tag,
        const std::vector<DropCase>& dropCases,
        std::string_view stem,
        std::string& error)
{
        LLVMBasicBlockRef done = appendBlock(std::string(stem) + ".done");
        LLVMValueRef instruction = LLVMBuildSwitch(
            builder_, tag, done, static_cast<unsigned>(dropCases.size()));
        std::vector<LLVMBasicBlockRef> blocks;
        blocks.reserve(dropCases.size());
        for (const DropCase& item : dropCases)
        {
            LLVMBasicBlockRef block = appendBlock(std::string(stem) + ".case");
            LLVMAddCase(instruction, LLVMConstInt(tagType, item.index, 0), block);
            blocks.push_back(block);
        }
        for (std::size_t ordinal = 0; ordinal < dropCases.size(); ++ordinal)
        {
            const DropCase& item = dropCases[ordinal];
            enterBlock(blocks[ordinal]);
            LLVMValueRef payloadAddress = byteOffset(
                address, payloadField.offset + item.field->offset,
                (std::string(stem) + ".payload.addr").c_str());
            const bool owned = item.symbol->declaredOwnership == sema::OwnershipKind::Owned &&
                (requiresOwnedDestruction(item.field->typeKey) ||
                 frontend::ProjectTypeConversion::isNativeArray(item.field->typeKey));
            if (owned)
            {
                LLVMTypeRef pointer = LLVMPointerTypeInContext(context_, 0);
                LLVMValueRef value = LLVMBuildLoad2(
                    builder_, pointer, payloadAddress,
                    (std::string(stem) + ".payload").c_str());
                LLVMBuildStore(builder_, LLVMConstNull(pointer), payloadAddress);
                if (!emitOwnedPointerDestroy(
                        value, item.field->typeKey,
                        std::string(stem) + ".payload.destroy", error)) return false;
            }
            else if (!emitValueDropAddress(
                         payloadAddress, item.field->typeKey,
                         std::string(stem) + ".payload", error))
                return false;
            if (!terminated()) LLVMBuildBr(builder_, done);
        }
        enterBlock(done);
        return true;
    }

bool OopLlvmImplementation::emitVariantDropAddress(
        LLVMValueRef address,
        const frontend::ProjectOopMirStructLayout& layout,
        std::string_view stem,
        std::string& error)
{
        if (layout.typeOrdinal >= nameLookup_.namedTypes().size() || memberModel_ == nullptr)
            return fail(error, "NativeSharp 3.0 variant drop has no nominal type model");
        const frontend::ObjectFieldLayout* tagField = nullptr;
        const frontend::ObjectFieldLayout* payloadField = nullptr;
        for (const frontend::ObjectFieldLayout& field : layout.fields)
        {
            if (field.memberOrdinal >= memberModel_->members().size()) continue;
            const frontend::ClassMemberSymbol& symbol = memberModel_->members()[field.memberOrdinal];
            if (symbol.name == "__nsc_variant_tag") tagField = &field;
            else if (symbol.name == "__nsc_variant_payload") payloadField = &field;
        }
        if (tagField == nullptr)
            return fail(error, "NativeSharp 3.0 variant drop lost its active-case tag");
        if (payloadField == nullptr) return true;
        const frontend::ProjectOopMirStructLayout* payloadLayout =
            findStructLayout(mirModel_, payloadField->typeKey);
        if (payloadLayout == nullptr || !payloadLayout->isUnion)
            return fail(error, "NativeSharp 3.0 variant drop lost its payload union layout");
        std::unordered_map<std::string, std::uint64_t> caseIndices;
        collectVariantCaseIndices(layout, caseIndices);
        std::vector<DropCase> dropCases;
        if (!collectVariantDropCases(*payloadLayout, caseIndices, dropCases, error)) return false;
        if (dropCases.empty()) return true;
        LLVMTypeRef tagType = llvmType(tagField->typeKey, error);
        if (tagType == nullptr || !error.empty()) return false;
        LLVMValueRef tagAddress = byteOffset(
            address, tagField->offset, (std::string(stem) + ".tag.addr").c_str());
        LLVMValueRef tag = LLVMBuildLoad2(
            builder_, tagType, tagAddress, (std::string(stem) + ".tag").c_str());
        return emitVariantDropCases(
            address, *payloadField, tagType, tag, dropCases, stem, error);
    }

bool OopLlvmImplementation::emitFixedArrayDropAddress(
        LLVMValueRef address,
        std::string_view typeKey,
        std::string_view stem,
        bool& handled,
        std::string& error)
{
        handled = false;
        if (const std::optional<std::string> element =
                frontend::ProjectMemberModel::fixedArrayElementTypeKey(typeKey))
        {
            handled = true;
            const std::optional<std::uint64_t> count =
                frontend::ProjectMemberModel::fixedArrayCount(typeKey);
            if (!count.has_value())
                return fail(error, "NativeSharp 3.0 value drop lost fixed-array extent");
            const auto [elementSize, ignoredAlignment] = captureValueLayout(*element, error);
            (void)ignoredAlignment;
            if (!error.empty() || elementSize == 0) return false;
            for (std::uint64_t index = *count; index-- > 0;)
            {
                LLVMValueRef elementAddress = byteOffset(
                    address, index * elementSize,
                    (std::string(stem) + ".element.addr").c_str());
                if (!emitValueDropAddress(
                        elementAddress, *element,
                        std::string(stem) + ".element", error)) return false;
            }
            return true;
        }
        return true;
    }

bool OopLlvmImplementation::emitStructValueDropMembers(
        LLVMValueRef address,
        const frontend::ProjectOopMirStructLayout& layout,
        std::string_view stem,
        std::string& error)
{
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(layout.typeOrdinal))
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const frontend::ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
            if (member.kind != frontend::ClassMemberKind::Destructor) continue;
            const frontend::ProjectOopMirFunction* destructor = mirModel_.functionForMember(memberOrdinal);
            if (destructor == nullptr)
                return fail(error, "NativeSharp 3.0 value destructor has no MIR function");
            const auto target = functions_.find(destructor->mangledName);
            if (target == functions_.end())
                return fail(error, "NativeSharp 3.0 value destructor was not declared: " +
                    destructor->mangledName);
            LLVMValueRef arguments[] = {address};
            buildNoUnwindCall(
                target->second.functionType, target->second.function, arguments, 1, "");
            break;
        }

        for (auto iterator = layout.fields.rbegin(); iterator != layout.fields.rend(); ++iterator)
        {
            const frontend::ObjectFieldLayout& field = *iterator;
            if (field.memberOrdinal >= memberModel_->members().size()) continue;
            const frontend::ClassMemberSymbol& symbol = memberModel_->members()[field.memberOrdinal];
            if (symbol.kind != frontend::ClassMemberKind::Field || symbol.isStatic) continue;
            LLVMValueRef fieldAddress = byteOffset(
                address, field.offset, (std::string(stem) + ".field.addr").c_str());
            const bool ownedPointer = symbol.declaredOwnership == sema::OwnershipKind::Owned &&
                (requiresOwnedDestruction(field.typeKey) ||
                 frontend::ProjectTypeConversion::isNativeArray(field.typeKey));
            if (ownedPointer)
            {
                LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
                LLVMValueRef value = LLVMBuildLoad2(
                    builder_, ptr, fieldAddress,
                    (std::string(stem) + ".field.load").c_str());
                LLVMBuildStore(builder_, LLVMConstNull(ptr), fieldAddress);
                if (!emitOwnedPointerDestroy(
                        value, field.typeKey,
                        std::string(stem) + ".field.destroy", error)) return false;
            }
            else if (memberModel_->typeRequiresValueDrop(field.typeKey))
            {
                if (!emitValueDropAddress(
                        fieldAddress, field.typeKey,
                        std::string(stem) + ".field", error)) return false;
            }
        }
        return true;
    }

bool OopLlvmImplementation::emitValueDropAddress(
        LLVMValueRef address,
        std::string_view typeKey,
        std::string_view stem,
        std::string& error,
        bool allowNamedThunk )
{
        if (address == nullptr || memberModel_ == nullptr)
            return fail(error, "NativeSharp 3.0 value drop has no address/member model");
        bool fixedArrayHandled = false;
        if (!emitFixedArrayDropAddress(
                address, typeKey, stem, fixedArrayHandled, error)) return false;
        if (fixedArrayHandled) return true;
        const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, typeKey);
        if (layout == nullptr) return true;
        if (allowNamedThunk && !layout->dropGlueEntryName.empty())
        {
            const auto entry = valueDropEntries_.find(layout->typeKey);
            if (entry == valueDropEntries_.end())
                return fail(error, "NativeSharp 3.0 value drop glue was not declared: " +
                    layout->dropGlueEntryName);
            LLVMValueRef arguments[] = {address};
            buildNoUnwindCall(
                entry->second.functionType, entry->second.function, arguments, 1, "");
            return true;
        }
        if (!layout->isUnion)
        {
            bool variant = false;
            for (const frontend::ObjectFieldLayout& field : layout->fields)
            {
                if (field.memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[field.memberOrdinal].name == "__nsc_variant_tag")
                {
                    variant = true;
                    break;
                }
            }
            if (variant) return emitVariantDropAddress(address, *layout, stem, error);
        }
        if (layout->isUnion)
            return fail(error, "NativeSharp 3.0 value drop requires an active-member discriminator for union '" +
                std::string(typeKey) + "'");
        return emitStructValueDropMembers(address, *layout, stem, error);
    }

bool OopLlvmImplementation::emitValueDrop(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        LLVMValueRef flag,
        std::string_view stem,
        std::string& error)
{
        if (node == nullptr || storage == nullptr)
            return fail(error, "NativeSharp 3.0 value cleanup is missing storage");
        if (flag == nullptr)
            return emitValueDropAddress(storage, node->typeKey, stem, error);

        LLVMTypeRef flagType = LLVMInt1TypeInContext(context_);
        LLVMValueRef live = LLVMBuildLoad2(
            builder_, flagType, flag, (std::string(stem) + ".live").c_str());
        LLVMBasicBlockRef destroyBlock = appendBlock(std::string(stem) + ".destroy");
        LLVMBasicBlockRef doneBlock = appendBlock(std::string(stem) + ".done");
        LLVMBuildCondBr(builder_, live, destroyBlock, doneBlock);
        enterBlock(destroyBlock);
        LLVMBuildStore(builder_, LLVMConstInt(flagType, 0, 0), flag);
        if (!emitValueDropAddress(storage, node->typeKey, stem, error)) return false;
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

bool OopLlvmImplementation::emitRaiiDestroy(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr ||
            (node->kind != frontend::ProjectOopMirNodeKind::LocalDeclaration &&
             node->kind != frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow) ||
            !node->localOrdinal.has_value() || !node->cleanupRegionOrdinal.has_value())
        {
            return fail(error, "Stage 49 RAII cleanup is missing local/region identity");
        }
        const auto found = localStorage_.find(*node->localOrdinal);
        if (found == localStorage_.end() || found->second == nullptr)
        {
            return fail(error, "Stage 49 RAII cleanup cannot find owner local storage");
        }
        if (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(node->typeKey))
        {
            const auto flag = localValueDropFlags_.find(*node->localOrdinal);
            return emitValueDrop(
                node, found->second,
                flag != localValueDropFlags_.end() ? flag->second : nullptr,
                "value.local", error);
        }
        // Defense in depth (closure checklist P1 item 5/6): semantic analysis already restricts a
        // `raii` local to a class/interface reference or `builtin:string` (see
        // ProjectMethodBodyModel.cpp's "raii local requires a NativeSharp class/interface
        // reference type or native string" diagnostic), so `node->typeKey` reaching here as a
        // value type should never actually happen. But Bug A's own lesson was precisely that an
        // upstream invariant everyone was confident held ("owned parameters storage is always a
        // pointer") turned out not to, so this LLVM layer no longer assumes it silently here
        // either -- if a future MIR change ever let a value-typed cleanup node through, this
        // still degrades to a safe no-op instead of loading/storing an alloca as a pointer.
        if (!requiresOwnedDestruction(node->typeKey) && !node->typeKey.ends_with("[]"))
        {
            return true;
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, found->second, "raii.owner.cleanup");
        LLVMBasicBlockRef destroyBlock = appendBlock("raii.destroy");
        LLVMBasicBlockRef doneBlock = appendBlock("raii.destroy.done");
        // Bug B-006 fix -- see emitCleanupGuardBranch()'s comment. `__C_specific_handler` really does
        // run an enclosing cleanup while unwinding to a catch in the same frame (verified against a
        // clang-built C SEH reference program under the same Wine, which double-runs the analogous
        // __finally identically -- this is the personality's documented behaviour, not a defect
        // unique to this compiler), so a raii object whose scope encloses a try that catches gets
        // its cleanup entered twice: once from the funclet during that unwind, once from the normal
        // scope exit. The volatile flag below is what turns "the cleanup may be entered twice" into
        // "the destructor runs exactly once".
        emitCleanupGuardBranch(found->second, value, destroyBlock, doneBlock, "raii.owner");
        bool handled = false;
        if (!emitRaiiSpecialDestroy(node, value, doneBlock, handled, error)) return false;
        if (handled) return true;
        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
        {
            return fail(error, "Stage 49 RAII cleanup has no class-descriptor destruction ABI");
        }
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, value, "raii.descriptor");
        // Owned storage dispatches through field 5. For an ordinary heap object that is the
        // descriptor's normal destroy-and-free entry; for a custom allocator object its sidecar
        // replaces field 5 with the context-aware release wrapper. Borrowed raw placement and
        // stack-promoted storage dispatch through field 7 and therefore destroy in place only.
        const bool releasesStorage =
            node->kind == frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow ||
            node->storageOwnership == sema::OwnershipKind::Owned;
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, releasesStorage ? 5U : 7U,
            releasesStorage ? "raii.release.entry.addr" : "raii.object.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress,
            releasesStorage ? "raii.release.entry" : "raii.object.entry");
        LLVMValueRef arguments[] = {value};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return false;
        }
        if (terminated())
        {
            return true;
        }
        LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

bool OopLlvmImplementation::emitRaiiSpecialDestroy(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef value,
        LLVMBasicBlockRef doneBlock,
        bool& handled,
        std::string& error)
{
        handled = true;
        if (isDelegateReferenceType(nameLookup_, node->typeKey))
        {
            if (!emitDelegateDestroy(value, "raii.delegate", error)) return false;
        }
        else if (frontend::ProjectTypeConversion::isNativeArray(node->typeKey))
        {
            if (!emitNativeArrayDestroy(
                    value,
                    node->typeKey,
                    "raii.array",
                    node->ownership != sema::OwnershipKind::Borrowed,
                    error)) return false;
        }
        else if (node->typeKey == "builtin:string")
        {
            LLVMValueRef arguments[] = {value};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            LLVMBuildBr(builder_, doneBlock);
            enterBlock(doneBlock);
            return true;
        }
        else
        {
            handled = false;
            return true;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

// EN: Freeze 3.0 C-14: the frame slot of a parameter a $poll/$moveNext captured, or null. The poll's
//     own parameter 0 is `self`; the original parameters live in the frame the constructor filled.
// ES: Congelamiento 3.0 C-14: el slot del frame de un parametro capturado por un $poll/$moveNext, o
//     null. El parametro 0 del poll es `self`; los parametros originales viven en el frame que llenó
//     el constructor.
LLVMValueRef OopLlvmImplementation::capturedParameterSlot(
        std::size_t parameterOrdinal, std::string_view typeKey)
{
        if (currentMirFunction_ == nullptr || currentMirFunction_->asyncFrameParameters.empty() ||
            parameterStorage_.empty()) return nullptr;
        const auto* captured = mirModel_.asyncFrameParameter(
            *currentMirFunction_, parameterOrdinal, std::string(typeKey));
        if (captured == nullptr) return nullptr;
        const auto& [ordinal, offset, capturedType] = *captured;
        if (ordinal != parameterOrdinal || capturedType != typeKey) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef self = LLVMBuildLoad2(builder_, ptr, parameterStorage_[0], "captured.self");
        if (currentMirFunction_->mangledName.ends_with("$moveNext$poll"))
            self = LLVMBuildLoad2(builder_, ptr, byteOffset(self, 8, "captured.iter.addr"), "captured.iter");
        LLVMValueRef frame = LLVMBuildLoad2(
            builder_, ptr, byteOffset(self, 8, "captured.frame.addr"), "captured.frame");
        return byteOffset(frame, offset, "captured.slot");
}

bool OopLlvmImplementation::destroyCapturedOwnedParameter(
        LLVMValueRef slot, std::string_view typeKey, std::string& error)
{
        if (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(typeKey))
            return emitValueDropAddress(slot, typeKey, "captured.value", error);
        if (!requiresOwnedDestruction(typeKey)) return true;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, slot, "captured.owned");
        // Clear first: the slot never reaches a second destruction.
        LLVMBuildStore(builder_, LLVMConstNull(ptr), slot);
        return emitOwnedPointerDestroy(value, typeKey, "captured.owned.destroy", error);
}

bool OopLlvmImplementation::emitOwnedParameterDestroy(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || node->kind != frontend::ProjectOopMirNodeKind::OwnedParameterCleanup ||
            !node->parameterOrdinal.has_value() || !node->cleanupRegionOrdinal.has_value())
        {
            return fail(error, "Stage 50.8 owned-parameter cleanup is missing parameter/region identity");
        }
        // EN: Freeze 3.0 C-14: in a poll the owned parameter lives in the frame; reading parameter
        //     0 here destroyed the future itself as if it were the parameter (a crash).
        // ES: Congelamiento 3.0 C-14: en un poll el parametro owned vive en el frame; leer el
        //     parametro 0 aqui destruia el future mismo como si fuera el parametro (un crash).
        if (LLVMValueRef captured = capturedParameterSlot(*node->parameterOrdinal, node->typeKey))
            return destroyCapturedOwnedParameter(captured, node->typeKey, error);
        const auto found = parameterStorage_.find(*node->parameterOrdinal);
        if (found == parameterStorage_.end() || found->second == nullptr)
        {
            return fail(error, "Stage 50.8 owned-parameter cleanup cannot find parameter storage");
        }
        if (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(node->typeKey))
        {
            const auto flag = parameterValueDropFlags_.find(*node->parameterOrdinal);
            return emitValueDrop(
                node, found->second,
                flag != parameterValueDropFlags_.end() ? flag->second : nullptr,
                "value.param", error);
        }
        // Foreach-driven regression find (52_x_nativesharp_foreach.md): an `owned` PARAMETER,
        // unlike a `raii` LOCAL, is legitimately allowed to be a value type (int, bool, a plain
        // struct, ...) -- "owned" is a harmless no-op for those (Stage 52 fix 1/2/3), which is
        // exactly what lets e.g. `List<T>.Add(owned T value)` compile and run for `T = int`. But
        // this function used to load/store through `found->second` UNCONDITIONALLY as an 8-byte
        // pointer, with no check at all -- correct for the reference-typed/string case this was
        // originally written for, but for an `owned int` parameter `found->second` is a 4-byte
        // `alloca i32`: the unconditional 8-byte load/store below read and wrote past the end of
        // that allocation, corrupting whatever stack memory happened to sit next to it (in
        // practice: silently truncating a caller's `count` field to 1 after any `owned`
        // value-type parameter method that also makes a nested call, e.g. exactly
        // `List<T>.Add`'s `EnsureCapacity(count + 1)` -- the bug foreach-over-List<T> testing
        // caught). This was ALSO the exact root cause identified (but, per
        // WORKLOG_CURRENT_STATE_52.md/WINE_EXECUTION_FINDINGS_STAGE52.md, never actually fixed in
        // source) by the Stage 52 closure's Wine execution finding.
        //
        // A value type owns no separate heap allocation for `owned` to release, so the correct
        // fix is not "load fewer bytes" but "there is nothing to destroy here at all": skip the
        // entire null-check/load/store/destructor-call sequence below whenever the parameter's
        // type does not require real destruction. Centralized in requiresOwnedDestruction() (P1
        // item 6 of the closure checklist) so this exact check can no longer silently diverge
        // from emitRaiiDestroy's and lowerDeallocation's own copies of it.
        if (!requiresOwnedDestruction(node->typeKey))
        {
            return true;
        }
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, found->second, "owned.param.cleanup");
        LLVMBasicBlockRef destroyBlock = appendBlock("owned.param.destroy");
        LLVMBasicBlockRef doneBlock = appendBlock("owned.param.destroy.done");
        // Bug B-006 fix -- identical guard, identical reason as emitRaiiDestroy above (an `owned`
        // parameter's cleanup region is reached from the very same exceptional cleanup chains a
        // `raii` local's is). Fixing only the `raii` side would leave `owned` parameters silently
        // double-destroyed in exactly the same shape.
        emitCleanupGuardBranch(found->second, value, destroyBlock, doneBlock, "owned.param");
        if (isDelegateReferenceType(nameLookup_, node->typeKey))
        {
            if (!emitDelegateDestroy(value, "owned.param.delegate", error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, doneBlock);
            enterBlock(doneBlock);
            return true;
        }
        if (frontend::ProjectTypeConversion::isNativeArray(node->typeKey))
        {
            if (!emitNativeArrayDestroy(
                    value, node->typeKey, "owned.param.array", true, error)) return false;
            if (!terminated()) LLVMBuildBr(builder_, doneBlock);
            enterBlock(doneBlock);
            return true;
        }
        if (node->typeKey == "builtin:string")
        {
            LLVMValueRef arguments[] = {value};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            LLVMBuildBr(builder_, doneBlock);
            enterBlock(doneBlock);
            return true;
        }
        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
        {
            return fail(error, "Stage 50.8 owned-parameter cleanup has no class-descriptor destruction ABI");
        }
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, value, "owned.param.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 5, "owned.param.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "owned.param.entry");
        LLVMValueRef arguments[] = {value};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return false;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

bool OopLlvmImplementation::emitCleanupAction(std::size_t ordinal, std::string& error)
{
        const auto found = cleanupRegions_.find(ordinal);
        if (found == cleanupRegions_.end() || found->second == nullptr)
        {
            return fail(error, "Stage 45.10 cannot resolve MIR cleanup action");
        }
        const frontend::ProjectOopMirNode* cleanup = found->second;
        if (cleanup->kind == frontend::ProjectOopMirNodeKind::UsingRegion)
        {
            return emitUsingDispose(cleanup, error);
        }
        if ((cleanup->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
             cleanup->kind == frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow) &&
            cleanup->cleanupRegionOrdinal.has_value())
        {
            return emitRaiiDestroy(cleanup, error);
        }
        if (cleanup->kind == frontend::ProjectOopMirNodeKind::OwnedParameterCleanup &&
            cleanup->cleanupRegionOrdinal.has_value())
        {
            return emitOwnedParameterDestroy(cleanup, error);
        }
        if (cleanup->kind == frontend::ProjectOopMirNodeKind::FinallyRegion)
        {
            if (cleanup->children.empty() || cleanup->children.front() == nullptr)
            {
                return fail(error, "Stage 45.10 finally cleanup has no body");
            }
            return lowerStatement(cleanup->children.front().get(), error);
        }
        return fail(error, "Stage 50.8 cleanup ordinal does not refer to using/finally/RAII/owned-parameter MIR");
    }

bool OopLlvmImplementation::emitOneStructuredCleanup(std::size_t ordinal, std::string& error)
{
        const std::optional<std::size_t> index = activeCleanupIndex(
            ordinal, activeCleanupRegions_);
        if (!index.has_value())
        {
            return fail(error, "Stage 45.10 structured exit references an inactive cleanup region");
        }
        activeCleanupRegions_.resize(*index);
        return emitCleanupAction(ordinal, error);
    }

bool OopLlvmImplementation::structuredExitLeavesCatchBody(
        bool isReturn, std::size_t targetLoopIndex) const noexcept
{
        if (activeCatchExceptions_.empty()) return false;
        if (isReturn) return true;
        if (loopStack_.empty()) return false;
        return targetLoopIndex < activeCatchExceptions_.back().loopDepthAtEntry;
    }

bool OopLlvmImplementation::emitCatchBodyExceptionClear(
        bool isReturn, std::size_t targetLoopIndex, std::string& error)
{
        LLVMBasicBlockRef leaveBlock = appendBlock("eh.catch.leave");
        LLVMBuildBr(builder_, leaveBlock);
        enterBlock(leaveBlock);
        buildNoUnwindCall(exceptionClearFunctionType_, exceptionClearFunction_, nullptr, 0, "");
        // EN: Freeze 3.0 C-01: every catch body this exit leaves releases its exception. The
        //     copy keeps the frames stable while the release calls are lowered.
        // ES: Congelamiento 3.0 C-01: cada cuerpo de catch que esta salida abandona libera su
        //     excepcion. La copia mantiene estables los marcos mientras se bajan las liberaciones.
        const std::vector<ActiveCatchBody> frames = activeCatchExceptions_;
        for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
        {
            if (!isReturn && targetLoopIndex >= frame->loopDepthAtEntry) break;
            if (frame->ownerSlot == nullptr) continue;
            if (!emitCaughtExceptionRelease(frame->ownerSlot, error)) return false;
            if (terminated()) return true;
        }
        return true;
    }

bool OopLlvmImplementation::emitCaughtExceptionRelease(LLVMValueRef ownerSlot, std::string& error)
{
        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
            return fail(error, "freeze 3.0 catch ownership has no descriptor ABI");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef owned = LLVMBuildLoad2(builder_, ptr, ownerSlot, "eh.catch.owned");
        LLVMBasicBlockRef releaseBlock = appendBlock("eh.catch.release");
        LLVMBasicBlockRef doneBlock = appendBlock("eh.catch.release.done");
        emitCleanupGuardBranch(ownerSlot, owned, releaseBlock, doneBlock, "eh.catch.owner");
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, owned, "eh.catch.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 5U, "eh.catch.release.entry.addr");
        LLVMValueRef releaseEntry = LLVMBuildLoad2(builder_, ptr, entryAddress, "eh.catch.release.entry");
        LLVMValueRef arguments[] = {owned};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, releaseEntry, arguments, 1, "", error) == nullptr &&
            !error.empty()) return false;
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
    }

bool OopLlvmImplementation::emitCatchOwnershipOnThrow(LLVMValueRef exception, std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        const std::vector<ActiveCatchBody> frames = activeCatchExceptions_;
        for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
        {
            if (frame->ownerSlot == nullptr) continue;
            // EN: Throwing the caught object itself (`throw;` or `throw e;`) hands it over.
            // ES: Lanzar el mismo objeto atrapado (`throw;` o `throw e;`) lo entrega.
            LLVMValueRef owned = LLVMBuildLoad2(builder_, ptr, frame->ownerSlot, "eh.catch.owned");
            LLVMValueRef same = LLVMBuildICmp(builder_, LLVMIntEQ, owned, exception, "eh.catch.rethrown");
            LLVMBasicBlockRef handOver = appendBlock("eh.catch.handover");
            LLVMBasicBlockRef next = appendBlock("eh.catch.handover.done");
            LLVMBuildCondBr(builder_, same, handOver, next);
            enterBlock(handOver);
            LLVMBuildStore(builder_, LLVMConstNull(ptr), frame->ownerSlot);
            LLVMBuildBr(builder_, next);
            enterBlock(next);
            // EN: A throw with no try of its own inside this body leaves it: the catch releases
            //     the exception it still owns before the new one propagates.
            // ES: Un throw sin un try propio dentro de este cuerpo sale de el: el catch libera la
            //     excepcion que todavia posee antes de que la nueva se propague.
            if (frame->handlerDepthAtEntry != exceptionHandlerStack_.size()) continue;
            if (!emitCaughtExceptionRelease(frame->ownerSlot, error)) return false;
            if (terminated()) return true;
        }
        return true;
    }

}
