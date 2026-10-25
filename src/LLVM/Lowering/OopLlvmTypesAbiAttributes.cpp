#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMTypeRef OopLlvmImplementation::llvmType(std::string_view typeKey, std::string& error) const
{
    std::string key(typeKey);
    const auto cached = llvmTypesByKey_.find(key);
    if (cached != llvmTypesByKey_.end()) return cached->second;
    LLVMTypeRef lowered = mapLlvmType(context_, nameLookup_, mirModel_, typeKey, error);
    if (lowered != nullptr) llvmTypesByKey_.try_emplace(std::move(key), lowered);
    return lowered;
}

bool OopLlvmImplementation::ensureAllocatorDescriptorSidecar(std::string& error)
{
        if (allocatorDescriptorSidecarType_ != nullptr &&
            allocatorDescriptorSidecarReleaseFunction_ != nullptr)
            return true;
        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr ||
            allocatorFreeThunkType_ == nullptr || deallocationFunction_ == nullptr)
        {
            return fail(error, "NS304-T13-F07 allocator sidecar requires completed descriptor/runtime ABI");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef sidecarFields[] = {descriptorType_, ptr, ptr};
        allocatorDescriptorSidecarType_ = LLVMStructCreateNamed(
            context_, "nativesharp.allocator_descriptor_sidecar");
        if (allocatorDescriptorSidecarType_ == nullptr)
            return fail(error, "NS304-T13-F07 failed to create allocator descriptor sidecar type");
        LLVMStructSetBody(allocatorDescriptorSidecarType_, sidecarFields, 3, 0);

        allocatorDescriptorSidecarReleaseFunction_ = LLVMAddFunction(
            module_, "__nativesharp_allocator_sidecar_destroy_and_free",
            destructionEntryFunctionType_);
        if (allocatorDescriptorSidecarReleaseFunction_ == nullptr)
            return fail(error, "NS304-T13-F07 failed to create allocator sidecar release entry");
        LLVMSetLinkage(allocatorDescriptorSidecarReleaseFunction_, LLVMInternalLinkage);

        LLVMBasicBlockRef savedBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            context_, allocatorDescriptorSidecarReleaseFunction_, "entry");
        enterBlock(entry);
        LLVMValueRef object = LLVMGetParam(allocatorDescriptorSidecarReleaseFunction_, 0);
        LLVMSetValueName2(object, "object", 6);
        LLVMValueRef sidecar = LLVMBuildLoad2(builder_, ptr, object, "allocator.sidecar");

        LLVMValueRef objectEntryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, sidecar, 7, "allocator.sidecar.object.entry.addr");
        LLVMValueRef objectEntry = LLVMBuildLoad2(
            builder_, ptr, objectEntryAddress, "allocator.sidecar.object.entry");
        LLVMValueRef destroyArguments[] = {object};
        LLVMBuildCall2(
            builder_, destructionEntryFunctionType_, objectEntry, destroyArguments, 1, "");

        LLVMValueRef contextAddress = LLVMBuildStructGEP2(
            builder_, allocatorDescriptorSidecarType_, sidecar, 1,
            "allocator.sidecar.context.addr");
        LLVMValueRef context = LLVMBuildLoad2(
            builder_, ptr, contextAddress, "allocator.sidecar.context");
        LLVMValueRef thunkAddress = LLVMBuildStructGEP2(
            builder_, allocatorDescriptorSidecarType_, sidecar, 2,
            "allocator.sidecar.thunk.addr");
        LLVMValueRef thunk = LLVMBuildLoad2(
            builder_, ptr, thunkAddress, "allocator.sidecar.thunk");
        LLVMValueRef deallocateArguments[] = {context, object};
        LLVMBuildCall2(
            builder_, allocatorFreeThunkType_, thunk, deallocateArguments, 2, "");
        LLVMValueRef freeSidecarArguments[] = {sidecar};
        LLVMBuildCall2(
            builder_, deallocationFunctionType_, deallocationFunction_,
            freeSidecarArguments, 1, "");
        LLVMBuildRetVoid(builder_);
        applyFunctionAttribute(allocatorDescriptorSidecarReleaseFunction_, "nounwind");

        if (savedBlock != nullptr) enterBlock(savedBlock);
        return true;
    }

bool OopLlvmImplementation::installAllocatorDescriptorSidecar(
        LLVMValueRef object,
        LLVMValueRef descriptor,
        LLVMValueRef context,
        LLVMValueRef thunk,
        std::string& error)
{
        if (object == nullptr || descriptor == nullptr || context == nullptr || thunk == nullptr ||
            !ensureAllocatorDescriptorSidecar(error))
            return false;

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Frozen Win64 descriptor ABI: six pointers + two i64 values = 64 bytes. The private
        // sidecar appends two pointers and is therefore 80 bytes. These constants deliberately
        // describe the public descriptor contract instead of querying host C++ layout.
        constexpr std::uint64_t descriptorBytes = 64;
        constexpr std::uint64_t sidecarBytes = 80;
        LLVMValueRef sidecarSize = LLVMConstInt(
            LLVMInt64TypeInContext(context_), sidecarBytes, 0);
        LLVMValueRef allocationArguments[] = {sidecarSize};
        LLVMValueRef sidecar = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, allocationArguments, 1,
            "allocator.sidecar");
        LLVMValueRef sidecarValid = LLVMBuildICmp(
            builder_, LLVMIntNE, sidecar, LLVMConstNull(ptr), "allocator.sidecar.valid");
        LLVMBasicBlockRef readyBlock = appendBlock("allocator.sidecar.ready");
        LLVMBasicBlockRef failureBlock = appendBlock("allocator.sidecar.failure");
        LLVMBuildCondBr(builder_, sidecarValid, readyBlock, failureBlock);

        enterBlock(failureBlock);
        LLVMValueRef releaseArguments[] = {context, object};
        buildNoUnwindCall(allocatorFreeThunkType_, thunk, releaseArguments, 2, "");
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::AllocationFailure), 0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);

        enterBlock(readyBlock);
        LLVMValueRef sidecarDescriptorAddress = LLVMBuildStructGEP2(
            builder_, allocatorDescriptorSidecarType_, sidecar, 0,
            "allocator.sidecar.descriptor.addr");
        LLVMValueRef descriptorSize = LLVMConstInt(
            LLVMInt64TypeInContext(context_), descriptorBytes, 0);
        LLVMValueRef copyArguments[] = {
            sidecarDescriptorAddress,
            descriptor,
            descriptorSize,
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memcpyFunctionType_, memcpyFunction_, copyArguments, 4, "");

        LLVMValueRef releaseEntryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, sidecarDescriptorAddress, 5,
            "allocator.sidecar.release.entry.addr");
        LLVMBuildStore(
            builder_, allocatorDescriptorSidecarReleaseFunction_, releaseEntryAddress);
        LLVMValueRef contextAddress = LLVMBuildStructGEP2(
            builder_, allocatorDescriptorSidecarType_, sidecar, 1,
            "allocator.sidecar.context.addr");
        LLVMBuildStore(builder_, context, contextAddress);
        LLVMValueRef thunkAddress = LLVMBuildStructGEP2(
            builder_, allocatorDescriptorSidecarType_, sidecar, 2,
            "allocator.sidecar.thunk.addr");
        LLVMBuildStore(builder_, thunk, thunkAddress);
        LLVMBuildStore(builder_, sidecar, object);
        return true;
    }

LLVMBasicBlockRef OopLlvmImplementation::emitAllocatorConstructionFailureCleanup(
        LLVMValueRef object,
        LLVMBasicBlockRef outerUnwind,
        std::string& error)
{
        if (!ensureAllocatorDescriptorSidecar(error)) return nullptr;
        LLVMBasicBlockRef sourceBlock = LLVMGetInsertBlock(builder_);
        if (sourceBlock == nullptr)
        {
            error = "NS304-T13-F07 constructor failure has no source block";
            return nullptr;
        }

        LLVMValueRef parentPad = activeFuncletPad();
        LLVMBasicBlockRef cleanupBlock = appendBlock("placement.ctor.failure");
        enterBlock(cleanupBlock);
        LLVMValueRef cleanupPad = LLVMBuildCleanupPad(
            builder_, parentPad, nullptr, 0, "placement.ctor.failure.pad");
        if (cleanupPad == nullptr)
        {
            enterBlock(sourceBlock);
            error = "NS304-T13-F07 failed to create constructor failure cleanuppad";
            return nullptr;
        }

        funcletStack_.push_back(FuncletContext{
            cleanupPad,
            FuncletKind::Cleanup,
            activeCleanupRegions_.size(),
            nullptr,
            outerUnwind});
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef sidecar = LLVMBuildLoad2(
            builder_, pointerType, object, "placement.failure.sidecar");
        LLVMValueRef thunkAddress = LLVMBuildStructGEP2(
            builder_, allocatorDescriptorSidecarType_, sidecar, 2,
            "placement.failure.thunk.addr");
        LLVMValueRef thunk =
            LLVMBuildLoad2(builder_, pointerType, thunkAddress, "placement.failure.thunk");
        LLVMValueRef contextAddress = LLVMBuildStructGEP2(
            builder_, allocatorDescriptorSidecarType_, sidecar, 1,
            "placement.failure.context.addr");
        LLVMValueRef context =
            LLVMBuildLoad2(builder_, pointerType, contextAddress, "placement.failure.context");
        LLVMValueRef deallocateArguments[] = {context, object};
        buildNoUnwindCall(
            allocatorFreeThunkType_, thunk, deallocateArguments, 2, "");
        LLVMValueRef freeSidecarArguments[] = {sidecar};
        buildNoUnwindCall(
            deallocationFunctionType_, deallocationFunction_, freeSidecarArguments, 1, "");
        LLVMBuildCleanupRet(builder_, cleanupPad, outerUnwind);
        funcletStack_.pop_back();
        enterBlock(sourceBlock);
        return cleanupBlock;
    }

LLVMValueRef OopLlvmImplementation::allocatorDeallocateThunk(
        std::size_t allocatorTypeOrdinal,
        std::string_view deallocateCalleeName,
        std::uint64_t objectSize,
        std::uint64_t objectAlignment,
        std::string& error)
{
        const std::string thunkName =
            "__nativesharp_allocator_deallocate_" + std::to_string(allocatorTypeOrdinal) + "_" +
            std::to_string(objectSize) + "_" + std::to_string(objectAlignment);
        const auto existing = allocatorThunks_.find(thunkName);
        if (existing != allocatorThunks_.end()) return existing->second;
        const auto target = functions_.find(std::string(deallocateCalleeName));
        if (target == functions_.end())
        {
            fail(error, "NS304-T13-F07 allocator Deallocate target not declared: " +
                std::string(deallocateCalleeName));
            return nullptr;
        }
        LLVMBasicBlockRef savedBlock = LLVMGetInsertBlock(builder_);
        LLVMTypeRef thunkParams[] = {
            LLVMPointerTypeInContext(context_, 0), LLVMPointerTypeInContext(context_, 0)};
        LLVMTypeRef thunkType =
            LLVMFunctionType(LLVMVoidTypeInContext(context_), thunkParams, 2, 0);
        LLVMValueRef thunk = LLVMAddFunction(module_, thunkName.c_str(), thunkType);
        LLVMSetLinkage(thunk, LLVMInternalLinkage);
        LLVMValueRef contextParam = LLVMGetParam(thunk, 0);
        LLVMValueRef pointerParam = LLVMGetParam(thunk, 1);
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context_, thunk, "entry");
        enterBlock(entry);
        LLVMValueRef args[] = {
            contextParam,
            pointerParam,
            LLVMConstInt(LLVMInt64TypeInContext(context_), objectSize, 0),
            LLVMConstInt(LLVMInt32TypeInContext(context_), objectAlignment, 0)};
        LLVMBuildCall2(builder_, target->second.functionType, target->second.function, args, 4, "");
        LLVMBuildRetVoid(builder_);
        applyFunctionAttribute(thunk, "nounwind");
        allocatorThunks_[thunkName] = thunk;
        if (savedBlock != nullptr)
            enterBlock(savedBlock);
        return thunk;
    }

bool OopLlvmImplementation::isStructValue(std::string_view typeKey) const noexcept
{
        return findStructLayout(mirModel_, typeKey) != nullptr;
    }

bool OopLlvmImplementation::requiresOwnedDestruction(std::string_view typeKey) const noexcept
{
        return typeKey == "builtin:string" ||
            frontend::ProjectTypeConversion::isNativeArray(typeKey) ||
            isClassReferenceType(nameLookup_, typeKey) || isDelegateReferenceType(nameLookup_, typeKey);
    }

std::optional<std::size_t> OopLlvmImplementation::resolveKnownSimdTypeOrdinal(
        std::string_view fullyQualifiedName) const noexcept
{
        for (const std::size_t ordinal : nameLookup_.typeOrdinalsForName(fullyQualifiedName))
        {
            if (ordinal >= nameLookup_.namedTypes().size()) continue;
            const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[ordinal];
            if (type.kind == frontend::NamedTypeDeclarationKind::Struct &&
                type.identity.fullyQualifiedNameView() == fullyQualifiedName)
            {
                return ordinal;
            }
        }
        return std::nullopt;
    }

void OopLlvmImplementation::initializeKnownSimdTypeOrdinals()
{
        float32x2TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Float32x2");
        float32x3TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Float32x3");
        float32x4TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Float32x4");
        int32x4TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Int32x4");
        int64x2TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Int64x2");
        float32x8TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Float32x8");
        float64x4TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Float64x4");
        int32x8TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Int32x8");
        int64x4TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Int64x4");
    }

std::optional<std::size_t> OopLlvmImplementation::simdTypeOrdinal(
        std::string_view typeKey) const noexcept
{
        if (!typeKey.starts_with("type:")) return std::nullopt;
        return nameLookup_.typeOrdinalForIdentity(typeKey.substr(5));
    }

bool OopLlvmImplementation::isFloat32x4TypeKey(std::string_view typeKey) const noexcept
{
        const std::optional<std::size_t> ordinal = simdTypeOrdinal(typeKey);
        return ordinal.has_value() && ordinal == float32x4TypeOrdinal_;
    }

unsigned OopLlvmImplementation::compactFloatVectorLaneCount(std::string_view typeKey) const noexcept
{
        const std::optional<std::size_t> ordinal = simdTypeOrdinal(typeKey);
        if (!ordinal.has_value()) return 0U;
        if (ordinal == float32x2TypeOrdinal_) return 2U;
        if (ordinal == float32x3TypeOrdinal_) return 3U;
        if (ordinal == float32x4TypeOrdinal_) return 4U;
        return 0U;
    }

unsigned OopLlvmImplementation::integerSimd128LaneBits(std::string_view typeKey) const noexcept
{
        const std::optional<std::size_t> ordinal = simdTypeOrdinal(typeKey);
        if (!ordinal.has_value()) return 0U;
        if (ordinal == int32x4TypeOrdinal_) return 32U;
        if (ordinal == int64x2TypeOrdinal_) return 64U;
        return 0U;
    }

unsigned OopLlvmImplementation::floatingSimd256LaneBits(std::string_view typeKey) const noexcept
{
        const std::optional<std::size_t> ordinal = simdTypeOrdinal(typeKey);
        if (!ordinal.has_value()) return 0U;
        if (ordinal == float32x8TypeOrdinal_) return 32U;
        if (ordinal == float64x4TypeOrdinal_) return 64U;
        return 0U;
    }

unsigned OopLlvmImplementation::integerSimd256LaneBits(std::string_view typeKey) const noexcept
{
        const std::optional<std::size_t> ordinal = simdTypeOrdinal(typeKey);
        if (!ordinal.has_value()) return 0U;
        if (ordinal == int32x8TypeOrdinal_) return 32U;
        if (ordinal == int64x4TypeOrdinal_) return 64U;
        return 0U;
    }

unsigned OopLlvmImplementation::vectorCallHvaArity(std::string_view typeKey) const noexcept
{
        if (isFloat32x4TypeKey(typeKey)) return 0U;
        const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, typeKey);
        if (layout == nullptr || layout->isUnion || layout->hasExplicitFieldOffsets ||
            layout->pack != 0U || layout->fields.empty() || layout->fields.size() > 4U ||
            layout->size != layout->fields.size() * 16U)
        {
            return 0U;
        }
        for (std::size_t index = 0; index < layout->fields.size(); ++index)
        {
            const frontend::ObjectFieldLayout& field = layout->fields[index];
            if (!isFloat32x4TypeKey(field.typeKey) || field.offset != index * 16U ||
                field.size != 16U || field.bitOffset.has_value() || field.bitWidth.has_value())
            {
                return 0U;
            }
        }
        return static_cast<unsigned>(layout->fields.size());
    }

LLVMTypeRef OopLlvmImplementation::vectorCallAbiType(std::string_view typeKey) const noexcept
{
        LLVMTypeRef vectorType = LLVMVectorType(LLVMFloatTypeInContext(context_), 4U);
        if (isFloat32x4TypeKey(typeKey)) return vectorType;
        const unsigned arity = vectorCallHvaArity(typeKey);
        if (arity == 0U) return nullptr;
        std::vector<LLVMTypeRef> elements(arity, vectorType);
        return LLVMStructTypeInContext(context_, elements.data(), arity, 0);
    }

LLVMValueRef OopLlvmImplementation::carrierToVectorCallAbi(
        LLVMValueRef value, std::string_view typeKey, std::string& error)
{
        LLVMTypeRef carrier = llvmType(typeKey, error);
        LLVMTypeRef abiType = vectorCallAbiType(typeKey);
        if (carrier == nullptr || abiType == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(carrier, 16U, "vectorcall.carrier.to.abi");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        LLVMSetAlignment(store, 16U);
        LLVMValueRef load = LLVMBuildLoad2(builder_, abiType, slot, "vectorcall.abi.value");
        LLVMSetAlignment(load, 16U);
        return load;
    }

LLVMValueRef OopLlvmImplementation::vectorCallAbiToCarrier(
        LLVMValueRef value, std::string_view typeKey, std::string& error)
{
        LLVMTypeRef carrier = llvmType(typeKey, error);
        LLVMTypeRef abiType = vectorCallAbiType(typeKey);
        if (carrier == nullptr || abiType == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(carrier, 16U, "vectorcall.abi.to.carrier");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        LLVMSetAlignment(store, 16U);
        LLVMValueRef load = LLVMBuildLoad2(builder_, carrier, slot, "vectorcall.carrier.value");
        LLVMSetAlignment(load, 16U);
        return load;
    }

bool OopLlvmImplementation::isIndirectStructTypeKey(std::string_view typeKey) const noexcept
{
        if (isEnumTypeKey(nameLookup_, typeKey)) return false;
        const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, typeKey);
        if (layout == nullptr) return false;
        return layout->abiPassing == frontend::ProjectOopMirAbiPassing::Indirect;
    }

unsigned OopLlvmImplementation::structAlignmentFor(std::string_view typeKey) const noexcept
{
        const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, typeKey);
        return layout != nullptr ? static_cast<unsigned>(layout->alignment) : 1U;
    }

LLVMValueRef OopLlvmImplementation::entryBlockAlloca(LLVMTypeRef type, unsigned alignment, const char* name)
{
        LLVMBasicBlockRef saved = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(currentFunction_);
        LLVMValueRef first = LLVMGetFirstInstruction(entry);
        if (first != nullptr) LLVMPositionBuilderBefore(builder_, first);
        else enterBlock(entry);
        LLVMValueRef slot = LLVMBuildAlloca(builder_, type, name);
        if (alignment > 1U) LLVMSetAlignment(slot, alignment);
        enterBlock(saved);
        return slot;
    }

LLVMTypeRef OopLlvmImplementation::abiParameterType(std::string_view typeKey, std::string& error) const
{
        if (isIndirectStructTypeKey(typeKey)) return LLVMPointerTypeInContext(context_, 0);
        return llvmType(typeKey, error);
    }

LLVMValueRef OopLlvmImplementation::wrapIndirectArgument(
    LLVMValueRef value,
    std::string_view typeKey,
    std::string& error)
{
        if (value == nullptr || !isIndirectStructTypeKey(typeKey)) return value;
        LLVMTypeRef carrier = llvmType(typeKey, error);
        if (carrier == nullptr) return nullptr;
        const unsigned alignment = structAlignmentFor(typeKey);
        LLVMValueRef copy = entryBlockAlloca(carrier, alignment, "abi.arg.copy");
        LLVMValueRef store = LLVMBuildStore(builder_, value, copy);
        if (alignment > 1U) LLVMSetAlignment(store, alignment);
        return copy;
    }

void OopLlvmImplementation::emitAbiReturn(LLVMValueRef value)
{
        if (sretStorage_ != nullptr)
        {
            LLVMBuildStore(builder_, value, sretStorage_);
            LLVMBuildRetVoid(builder_);
            return;
        }
        LLVMBuildRet(builder_, value);
    }

LLVMBasicBlockRef OopLlvmImplementation::appendBlock(const std::string& stem)
{
        const std::string name = stem + "." + std::to_string(blockCounter_++);
        return LLVMAppendBasicBlockInContext(context_, currentFunction_, name.c_str());
    }

void OopLlvmImplementation::enterBlock(LLVMBasicBlockRef block)
{
        if (block != nullptr && LLVMGetFirstInstruction(block) == nullptr)
        {
            LLVMValueRef function = LLVMGetBasicBlockParent(block);
            LLVMBasicBlockRef last = function != nullptr ? LLVMGetLastBasicBlock(function) : nullptr;
            if (last != nullptr && last != block && block != LLVMGetEntryBasicBlock(function))
                LLVMMoveBasicBlockAfter(block, last);
        }
        LLVMPositionBuilderAtEnd(builder_, block);
    }

bool OopLlvmImplementation::emitDecimalScaleOverflow(LLVMValueRef effectiveScale, const char* stem, std::string& error)
{
        LLVMTypeRef wide = LLVMIntTypeInContext(context_, 128);
        LLVMValueRef scale28 = LLVMConstInt(wide, 28, 0);
        LLVMValueRef overflow = LLVMBuildICmp(
            builder_, LLVMIntUGT, effectiveScale, scale28,
            (std::string(stem) + ".overflow").c_str());
        LLVMBasicBlockRef okBlock = appendBlock(std::string(stem) + ".ok");
        LLVMBasicBlockRef trapBlock = appendBlock(std::string(stem) + ".trap");
        LLVMBuildCondBr(builder_, overflow, trapBlock, okBlock);
        enterBlock(trapBlock);
        if (trapFunction_ == nullptr || trapFunctionType_ == nullptr)
            return fail(error, "decimal scale overflow but no trap is configured");
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::DecimalOverflow), 0);
        LLVMValueRef trapArguments[] = { reason };
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
        enterBlock(okBlock);
        return true;
    }

LLVMValueRef OopLlvmImplementation::buildPow10(LLVMValueRef exponent, const char* stem)
{
        LLVMTypeRef lane = LLVMInt64TypeInContext(context_);
        LLVMBasicBlockRef preheader = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef header = appendBlock(std::string(stem) + ".header");
        LLVMBasicBlockRef body = appendBlock(std::string(stem) + ".body");
        LLVMBasicBlockRef exit = appendBlock(std::string(stem) + ".exit");
        LLVMValueRef one = LLVMConstInt(lane, 1, 0);
        LLVMValueRef ten = LLVMConstInt(lane, 10, 0);
        LLVMBuildBr(builder_, header);
        enterBlock(header);
        LLVMValueRef accumulator = LLVMBuildPhi(builder_, lane, "decimal.pow10.acc");
        LLVMValueRef remaining = LLVMBuildPhi(builder_, lane, "decimal.pow10.rem");
        LLVMValueRef done = LLVMBuildICmp(
            builder_, LLVMIntEQ, remaining, LLVMConstInt(lane, 0, 0), "decimal.pow10.done");
        LLVMBuildCondBr(builder_, done, exit, body);
        enterBlock(body);
        LLVMValueRef nextAcc = LLVMBuildMul(builder_, accumulator, ten, "decimal.pow10.mul");
        LLVMValueRef nextRem = LLVMBuildSub(builder_, remaining, one, "decimal.pow10.dec");
        LLVMBuildBr(builder_, header);
        LLVMAddIncoming(accumulator, &one, &preheader, 1);
        LLVMAddIncoming(accumulator, &nextAcc, &body, 1);
        LLVMAddIncoming(remaining, &exponent, &preheader, 1);
        LLVMAddIncoming(remaining, &nextRem, &body, 1);
        enterBlock(exit);
        return accumulator;
    }

void OopLlvmImplementation::applyFunctionAttribute(LLVMValueRef function, const char* attributeName)
{
        if (function == nullptr || context_ == nullptr || attributeName == nullptr) return;
        const unsigned kindId = LLVMGetEnumAttributeKindForName(
            attributeName, std::strlen(attributeName));
        if (kindId == 0) return;
        LLVMAttributeRef attribute = LLVMCreateEnumAttribute(context_, kindId, 0);
        if (attribute == nullptr) return;
        LLVMAddAttributeAtIndex(function, static_cast<LLVMAttributeIndex>(~0U), attribute);
    }

void OopLlvmImplementation::applyUwtableAttribute(LLVMValueRef function)
{
        if (function == nullptr || context_ == nullptr) return;
        static const unsigned kindId = LLVMGetEnumAttributeKindForName("uwtable", 7);
        if (kindId == 0) return;
        // 2 = UWTableKind::Async. Ver la medicion de arriba.
        LLVMAttributeRef attribute = LLVMCreateEnumAttribute(context_, kindId, 2);
        if (attribute == nullptr) return;
        LLVMAddAttributeAtIndex(function, static_cast<LLVMAttributeIndex>(~0U), attribute);
    }

void OopLlvmImplementation::applyDenormalFloatingPointAttribute(LLVMValueRef function)
{
        if (function == nullptr || context_ == nullptr) return;
        if (activeFloatingPointDenormalMode() == FloatingPointDenormalMode::Preserve) return;
        LLVMDenormalModeKind output = LLVMDenormalModeKindIEEE;
        LLVMDenormalModeKind input = LLVMDenormalModeKindIEEE;
        switch (activeFloatingPointDenormalMode())
        {
            case FloatingPointDenormalMode::Preserve: break;
            case FloatingPointDenormalMode::FlushToZero:
                output = LLVMDenormalModeKindPreserveSign;
                break;
            case FloatingPointDenormalMode::DenormalsAreZero:
                input = LLVMDenormalModeKindPreserveSign;
                break;
            case FloatingPointDenormalMode::FlushToZeroAndDenormalsAreZero:
                output = LLVMDenormalModeKindPreserveSign;
                input = LLVMDenormalModeKindPreserveSign;
                break;
        }
        LLVMAttributeRef attribute = LLVMCreateDenormalFPEnvAttribute(
            context_, output, input, output, input);
        if (attribute != nullptr)
            LLVMAddAttributeAtIndex(function, static_cast<LLVMAttributeIndex>(~0U), attribute);
    }

bool OopLlvmImplementation::terminated() const noexcept
{
        LLVMBasicBlockRef block = LLVMGetInsertBlock(builder_);
        return block != nullptr && LLVMGetBasicBlockTerminator(block) != nullptr;
    }

bool OopLlvmImplementation::fail(std::string& error, std::string message)
{
        error = std::move(message);
        return false;
    }

LLVMValueRef OopLlvmImplementation::failValue(std::string& error, std::string message)
{
        error = std::move(message);
        return nullptr;
    }

}
