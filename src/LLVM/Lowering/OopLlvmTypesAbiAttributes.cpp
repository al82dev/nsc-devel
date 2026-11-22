#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

#include <algorithm>
#include <limits>

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
        float64x2TypeOrdinal_ = resolveKnownSimdTypeOrdinal(
            "NativeSharp.Core.Numerics.Float64x2");
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

bool OopLlvmImplementation::simdCarrierShape(
        std::string_view typeKey, unsigned& lanes, unsigned& laneBits, bool& floating) const noexcept
{
        const std::optional<std::size_t> ordinal = simdTypeOrdinal(typeKey);
        if (!ordinal.has_value()) return false;
        struct Shape
        {
            const std::optional<std::size_t>* ordinal;
            unsigned lanes;
            unsigned laneBits;
            bool floating;
        };
        const Shape shapes[] = {
            {&float32x4TypeOrdinal_, 4U, 32U, true}, {&float64x2TypeOrdinal_, 2U, 64U, true},
            {&int32x4TypeOrdinal_, 4U, 32U, false}, {&int64x2TypeOrdinal_, 2U, 64U, false},
            {&float32x8TypeOrdinal_, 8U, 32U, true}, {&float64x4TypeOrdinal_, 4U, 64U, true},
            {&int32x8TypeOrdinal_, 8U, 32U, false}, {&int64x4TypeOrdinal_, 4U, 64U, false},
        };
        for (const Shape& shape : shapes)
        {
            if (!shape.ordinal->has_value() || **shape.ordinal != *ordinal) continue;
            lanes = shape.lanes;
            laneBits = shape.laneBits;
            floating = shape.floating;
            return true;
        }
        return false;
    }

LLVMTypeRef OopLlvmImplementation::simdCarrierVectorType(std::string_view typeKey) const
{
        unsigned lanes = 0;
        unsigned laneBits = 0;
        bool floating = false;
        if (!simdCarrierShape(typeKey, lanes, laneBits, floating)) return nullptr;
        LLVMTypeRef lane = floating
            ? (laneBits == 32U ? LLVMFloatTypeInContext(context_) : LLVMDoubleTypeInContext(context_))
            : LLVMIntTypeInContext(context_, laneBits);
        return LLVMVectorType(lane, lanes);
    }

bool OopLlvmImplementation::planVectorMatchesCarrier(
        const interop::abi_plan::Type& type, std::string_view typeKey) const
{
        using interop::abi_plan::TypeKind;
        unsigned lanes = 0;
        unsigned laneBits = 0;
        bool floating = false;
        if (!simdCarrierShape(typeKey, lanes, laneBits, floating)) return false;
        if (type.kind != TypeKind::Vector || type.widthOrCount != lanes || type.elements.size() != 1) return false;
        const auto& lane = type.elements[0];
        if (floating) return lane.kind == (laneBits == 32U ? TypeKind::Float : TypeKind::Double);
        return lane.kind == TypeKind::Integer && lane.widthOrCount == laneBits;
    }

unsigned OopLlvmImplementation::vectorSlotAlignment(std::string_view typeKey) const noexcept
{
        const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, typeKey);
        if (layout != nullptr && layout->size > 32U) return 64U;
        if (layout != nullptr && layout->size > 16U) return 32U;
        return 16U;
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

std::optional<OopLlvmImplementation::VectorCallAggregate> OopLlvmImplementation::vectorCallAggregate(
        std::string_view typeKey) const
{
        // EN: An iterative walk over nested records: each frame sums (struct) or maximizes (union) its members.
        // ES: Un recorrido iterativo por registros anidados: cada marco suma (struct) o maximiza (union).
        struct Frame
        {
            const frontend::ProjectOopMirStructLayout* layout = nullptr;
            std::size_t next = 0;
            std::uint64_t members = 0;
            std::uint64_t multiplier = 1;
            std::uint64_t end = 0;
        };
        VectorCallAggregate result;
        const auto sameBase = [&](unsigned bytes, bool vector)
        {
            if (result.baseBytes == 0U)
            {
                result.baseBytes = bytes;
                result.vector = vector;
                return true;
            }
            return result.baseBytes == bytes && result.vector == vector;
        };
        const frontend::ProjectOopMirStructLayout* root = findStructLayout(mirModel_, typeKey);
        if (root == nullptr || simdCarrierVectorType(typeKey) != nullptr || isEnumTypeKey(nameLookup_, typeKey))
            return std::nullopt;
        std::vector<Frame> frames;
        frames.push_back(Frame{root, 0, 0, 1, 0});
        std::uint64_t rootMembers = 0;
        while (!frames.empty())
        {
            Frame& frame = frames.back();
            if (frame.next == frame.layout->fields.size())
            {
                if (frame.members == 0U) return std::nullopt;
                const std::uint64_t members = frame.members * frame.multiplier;
                frames.pop_back();
                if (frames.empty()) rootMembers = members;
                else if (frames.back().layout->isUnion)
                    frames.back().members = std::max(frames.back().members, members);
                else frames.back().members += members;
                if (members > 4U || (!frames.empty() && frames.back().members > 4U)) return std::nullopt;
                continue;
            }
            const frontend::ObjectFieldLayout& field = frame.layout->fields[frame.next++];
            if (field.bitOffset.has_value() || field.bitWidth.has_value()) return std::nullopt;
            // EN: T19-036. A C++ field of Clang vectors of 128, 256 or 512 bits (`__m128`, `__m256`, `__m512` and
            //     their double and integer forms), alone or in an array, is seen by NativeSharp as an array of its
            //     lanes (T14 vector view); for vectorcall each vector is one member of the aggregate, as in Clang
            //     (isX86VectorTypeForVectorCall accepts these three sizes). All members have the same size; the
            //     wider ones need the CPU level of their registers, which declareFunction checks.
            // ES: T19-036. Un campo C++ de vectores de Clang de 128, 256 o 512 bits, solo o en un arreglo, NativeSharp
            //     lo ve como un arreglo de sus carriles (vista de vector de T14); para vectorcall cada vector es un
            //     miembro del agregado, como en Clang (isX86VectorTypeForVectorCall acepta esos tres tamanos). Todos
            //     los miembros tienen el mismo tamano; los mas anchos necesitan el nivel de CPU de sus registros,
            //     que comprueba declareFunction.
            if (memberModel_ != nullptr && field.memberOrdinal < memberModel_->members().size())
            {
                const frontend::ClassMemberSymbol& member = memberModel_->members()[field.memberOrdinal];
                if (member.isCppExternal && !member.cppArrayPhysicalLayouts.empty())
                {
                    const auto [vectorBytes, vectorAlignment] = member.cppArrayPhysicalLayouts.back();
                    if ((vectorBytes != 16U && vectorBytes != 32U && vectorBytes != 64U) ||
                        vectorAlignment != vectorBytes || field.size == 0U || field.size % vectorBytes != 0U)
                        return std::nullopt;
                    const std::uint64_t vectors = field.size / vectorBytes;
                    if (vectors > 4U || !sameBase(static_cast<unsigned>(vectorBytes), true)) return std::nullopt;
                    if (!frame.layout->isUnion)
                    {
                        if (field.offset < frame.end) return std::nullopt;
                        frame.end = field.offset + field.size;
                        frame.members += vectors;
                    }
                    else frame.members = std::max(frame.members, vectors);
                    if (frame.members > 4U) return std::nullopt;
                    continue;
                }
            }
            // EN: struct members may not overlap; together with the size check this rules out padding.
            // ES: los miembros de un struct no se solapan; con la comprobacion de tamano, no hay relleno.
            if (!frame.layout->isUnion)
            {
                if (field.offset < frame.end) return std::nullopt;
                frame.end = field.offset + field.size;
            }
            std::string element = field.typeKey;
            std::uint64_t count = 1;
            while (const auto inner = frontend::ProjectMemberModel::fixedArrayElementTypeKey(element))
            {
                const auto extent = frontend::ProjectMemberModel::fixedArrayCount(element);
                if (!extent || *extent == 0U || *extent > 4U || count * *extent > 4U) return std::nullopt;
                count *= *extent;
                element = *inner;
            }
            bool base = false;
            if (isFloat32x4TypeKey(element)) base = sameBase(16U, true);
            else if (element == "builtin:float") base = sameBase(4U, false);
            else if (element == "builtin:double") base = sameBase(8U, false);
            else
            {
                const frontend::ProjectOopMirStructLayout* nested = findStructLayout(mirModel_, element);
                if (nested == nullptr || isEnumTypeKey(nameLookup_, element)) return std::nullopt;
                frames.push_back(Frame{nested, 0, 0, count, 0});
                continue;
            }
            if (!base) return std::nullopt;
            if (frame.layout->isUnion) frame.members = std::max(frame.members, count);
            else frame.members += count;
            if (frame.members > 4U) return std::nullopt;
        }
        if (rootMembers == 0U || rootMembers > 4U || root->size != rootMembers * result.baseBytes)
            return std::nullopt;
        result.members = static_cast<unsigned>(rootMembers);
        return result;
    }

unsigned OopLlvmImplementation::vectorCallHvaArity(std::string_view typeKey) const
{
        const std::optional<VectorCallAggregate> aggregate = vectorCallAggregate(typeKey);
        return aggregate.has_value() ? aggregate->members : 0U;
    }

LLVMTypeRef OopLlvmImplementation::vectorCallAbiType(std::string_view typeKey) const
{
        // EN: T19-036. A NativeSharp.Core.Numerics carrier travels as the vector of its lanes (XMM or YMM).
        // ES: T19-036. Un portador de NativeSharp.Core.Numerics viaja como el vector de sus carriles.
        if (LLVMTypeRef carrierVector = simdCarrierVectorType(typeKey)) return carrierVector;
        LLVMTypeRef vectorType = LLVMVectorType(LLVMFloatTypeInContext(context_), 4U);
        const std::optional<VectorCallAggregate> aggregate = vectorCallAggregate(typeKey);
        if (!aggregate.has_value()) return nullptr;
        // EN: a vector member is carried as float lanes of its width (16, 32 or 64 bytes); the value goes through
        //     memory between the NativeSharp carrier and this type. / ES: un miembro vectorial viaja como carriles
        //     float de su ancho (16, 32 o 64 bytes); el valor pasa por memoria entre el portador y este tipo.
        LLVMTypeRef wideVectorType = LLVMVectorType(LLVMFloatTypeInContext(context_), aggregate->baseBytes / 4U);
        LLVMTypeRef base = aggregate->vector
            ? (aggregate->baseBytes == 16U ? vectorType : wideVectorType)
            : aggregate->baseBytes == 4U ? LLVMFloatTypeInContext(context_) : LLVMDoubleTypeInContext(context_);
        std::vector<LLVMTypeRef> elements(aggregate->members, base);
        return LLVMStructTypeInContext(context_, elements.data(), aggregate->members, 0);
    }

std::vector<OopLlvmImplementation::VectorCallPassing> OopLlvmImplementation::vectorCallPassing(
        const std::vector<std::string>& argumentTypeKeys, std::size_t fixedParameterCount) const
{
        // EN: Clang, WinX86_64ABIInfo::computeVectorCallArgs: six XMM registers. First pass: among the first six
        //     arguments, each float, double or vector takes one while any is left (a vector without one goes by
        //     reference, the ordinary rule); aggregates wait. Second pass: each aggregate, in order, takes as
        //     many as it has members while enough are left, else it goes by reference.
        // ES: Clang: seis registros XMM. Primera pasada: entre los seis primeros argumentos, cada float, double
        //     o vector toma uno mientras quede; los agregados esperan. Segunda pasada: cada agregado, en orden,
        //     toma tantos como miembros tenga si alcanzan; si no, va por referencia.
        std::vector<VectorCallPassing> passing(fixedParameterCount, VectorCallPassing::Ordinary);
        std::vector<unsigned> arity(fixedParameterCount, 0U);
        unsigned freeRegisters = 6U;
        for (std::size_t index = 0; index < fixedParameterCount; ++index)
        {
            const std::string& typeKey = argumentTypeKeys[index];
            arity[index] = vectorCallHvaArity(typeKey);
            if (arity[index] != 0U)
            {
                passing[index] = VectorCallPassing::HvaIndirect;
                continue;
            }
            const bool vector = simdCarrierVectorType(typeKey) != nullptr;
            const bool floating = typeKey == "builtin:float" || typeKey == "builtin:double";
            if (index < 6U && (vector || floating) && freeRegisters > 0U)
            {
                --freeRegisters;
                if (vector) passing[index] = VectorCallPassing::Vector;
            }
        }
        for (std::size_t index = 0; index < fixedParameterCount; ++index)
        {
            if (passing[index] != VectorCallPassing::HvaIndirect || freeRegisters < arity[index]) continue;
            freeRegisters -= arity[index];
            passing[index] = VectorCallPassing::HvaInReg;
        }
        return passing;
    }

LLVMValueRef OopLlvmImplementation::vectorCallArgument(
        LLVMValueRef carrier, std::string_view typeKey, VectorCallPassing passing, std::string& error)
{
        if (passing == VectorCallPassing::Vector || passing == VectorCallPassing::HvaInReg)
            return carrierToVectorCallAbi(carrier, typeKey, error);
        if (passing == VectorCallPassing::HvaIndirect)
        {
            // EN: always a pointer to a per-call copy, even for an 8-byte aggregate (Clang: indirect, not byval).
            // ES: siempre un puntero a una copia por llamada, aun para un agregado de 8 bytes.
            LLVMTypeRef type = llvmType(typeKey, error);
            if (type == nullptr) return nullptr;
            LLVMValueRef slot = entryBlockAlloca(
                type, std::max(16U, structAlignmentFor(typeKey)), "vectorcall.hva.copy");
            LLVMBuildStore(builder_, carrier, slot);
            return slot;
        }
        return wrapIndirectArgument(carrier, typeKey, error);
    }

LLVMValueRef OopLlvmImplementation::carrierToVectorCallAbi(
        LLVMValueRef value, std::string_view typeKey, std::string& error)
{
        LLVMTypeRef carrier = llvmType(typeKey, error);
        LLVMTypeRef abiType = vectorCallAbiType(typeKey);
        if (carrier == nullptr || abiType == nullptr) return nullptr;
        const unsigned alignment = vectorSlotAlignment(typeKey);
        LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "vectorcall.carrier.to.abi");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        LLVMSetAlignment(store, alignment);
        LLVMValueRef load = LLVMBuildLoad2(builder_, abiType, slot, "vectorcall.abi.value");
        LLVMSetAlignment(load, alignment);
        return load;
    }

bool OopLlvmImplementation::cVectorPlanValues(const frontend::ProjectOopMirFunction& function,
        std::vector<LLVMTypeRef>& parameters, LLVMTypeRef& result) const
{
        parameters.clear();
        result = nullptr;
        // EN: C3-01. A C `_Float16` (the Half carrier) travels in XMM as LLVM `half`, as Clang passes and returns it
        //     on Windows x64; a NativeSharp Half by value would use a general register. It needs no plan.
        // ES: C3-01. Un `_Float16` de C (el portador Half) viaja en XMM como `half` de LLVM, como lo pasa y devuelve
        //     Clang en Windows x64; un Half NativeSharp por valor usaria un registro general. No necesita plan.
        bool halfValues = false;
        if (function.isImportedC && !function.cppVectorCall)
        {
            parameters.assign(function.parameterTypeKeys.size(), nullptr);
            for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
                if (function.parameterTypeKeys[index] == interop::kCppHalfTypeKey)
                {
                    parameters[index] = LLVMHalfTypeInContext(context_);
                    halfValues = true;
                }
            if (function.returnTypeKey == interop::kCppHalfTypeKey)
            {
                result = LLVMHalfTypeInContext(context_);
                halfValues = true;
            }
            // EN: C3-02. A `_BitInt(N)` narrower than its carrier travels as LLVM `iN`, as Clang declares it; the call
            //     truncates the carrier and extends the result by the carrier's signedness.
            // ES: C3-02. Un `_BitInt(N)` mas estrecho que su portador viaja como `iN` de LLVM, como lo declara Clang;
            //     la llamada trunca el portador y extiende el resultado segun el signo del portador.
            if (function.cppBitIntWidths.size() == function.parameterTypeKeys.size() + 1U)
            {
                for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
                    if (function.cppBitIntWidths[index] != 0U)
                    {
                        parameters[index] = LLVMIntTypeInContext(context_, function.cppBitIntWidths[index]);
                        halfValues = true;
                    }
                if (function.cppBitIntWidths.back() != 0U)
                {
                    result = LLVMIntTypeInContext(context_, function.cppBitIntWidths.back());
                    halfValues = true;
                }
            }
            if (!halfValues) parameters.clear();
        }
        const interop::abi_plan::Plan* plan = function.cppAbiPlan.get();
        if (halfValues && (plan == nullptr || plan->variadic ||
            plan->logicalParameters.size() != function.parameterTypeKeys.size())) return true;
        if (!function.isImportedC || function.cppVectorCall || plan == nullptr || plan->variadic ||
            plan->logicalParameters.size() != function.parameterTypeKeys.size() ||
            std::any_of(function.cppObjectSizeModes.begin(), function.cppObjectSizeModes.end(),
                [](auto mode) { return mode != 0; }))
            return false;
        // EN: the 128-bit and 256-bit carriers of cppVectorValueType, with the LLVM vector type of their lanes.
        // ES: los portadores de 128 y 256 bits de cppVectorValueType, con el tipo vector LLVM de sus carriles.
        const auto vectorType = [&](const interop::abi_plan::Type& type, std::string_view typeKey) -> LLVMTypeRef
        {
            return planVectorMatchesCarrier(type, typeKey) ? simdCarrierVectorType(typeKey) : nullptr;
        };
        // EN: T19-004. A 128-bit integer: Windows x64 passes it by the address of a 16-byte aligned copy and returns
        //     it in XMM0 as <2 x i64>, as Clang's plan says; an LLVM `i128` would use two general registers.
        // ES: T19-004. Un entero de 128 bits: Windows x64 lo pasa por la direccion de una copia alineada a 16 bytes y
        //     lo devuelve en XMM0 como <2 x i64>, como dice el plan de Clang; un `i128` de LLVM usaria dos registros
        //     generales.
        const auto wideInteger = [](std::string_view typeKey) {
            return typeKey == "builtin:i128" || typeKey == "builtin:u128";
        };
        bool any = halfValues;
        if (!halfValues) parameters.assign(function.parameterTypeKeys.size(), nullptr);
        for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
        {
            if (parameters[index] != nullptr) continue;
            const auto& logical = plan->logicalParameters[index];
            if (wideInteger(function.parameterTypeKeys[index]) &&
                logical.passing == interop::abi_plan::Passing::Indirect)
            {
                parameters[index] = LLVMPointerTypeInContext(context_, 0);
                any = true;
                continue;
            }
            if (logical.passing != interop::abi_plan::Passing::Direct || logical.physicalCount != 1 ||
                logical.firstPhysical >= plan->physicalParameters.size()) continue;
            parameters[index] = vectorType(
                plan->physicalParameters[logical.firstPhysical].type, function.parameterTypeKeys[index]);
            any = any || parameters[index] != nullptr;
        }
        if (result == nullptr && plan->logicalReturn.passing == interop::abi_plan::Passing::Direct)
        {
            if (wideInteger(function.returnTypeKey) &&
                plan->physicalReturn.kind == interop::abi_plan::TypeKind::Vector &&
                plan->physicalReturn.widthOrCount == 2 && plan->physicalReturn.elements.size() == 1 &&
                plan->physicalReturn.elements[0].kind == interop::abi_plan::TypeKind::Integer &&
                plan->physicalReturn.elements[0].widthOrCount == 64)
                result = LLVMVectorType(LLVMInt64TypeInContext(context_), 2);
            else result = vectorType(plan->physicalReturn, function.returnTypeKey);
        }
        return any || result != nullptr;
    }

LLVMValueRef OopLlvmImplementation::carrierToAbiValue(
        LLVMValueRef value, std::string_view typeKey, LLVMTypeRef abiType, std::string& error)
{
        LLVMTypeRef carrier = llvmType(typeKey, error);
        if (carrier == nullptr || abiType == nullptr) return nullptr;
        // C3-02: an integer carrier narrows to `iN`. / Un portador entero se estrecha a `iN`.
        if (LLVMGetTypeKind(carrier) == LLVMIntegerTypeKind && LLVMGetTypeKind(abiType) == LLVMIntegerTypeKind &&
            LLVMGetIntTypeWidth(abiType) < LLVMGetIntTypeWidth(carrier))
            return LLVMBuildTrunc(builder_, value, abiType, "c.bitint.arg");
        const unsigned alignment = vectorSlotAlignment(typeKey);
        LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "c.vector.carrier.to.abi");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        LLVMSetAlignment(store, alignment);
        LLVMValueRef load = LLVMBuildLoad2(builder_, abiType, slot, "c.vector.abi.value");
        LLVMSetAlignment(load, alignment);
        return load;
    }

LLVMValueRef OopLlvmImplementation::abiValueToCarrier(
        LLVMValueRef value, std::string_view typeKey, LLVMTypeRef abiType, std::string& error)
{
        LLVMTypeRef carrier = llvmType(typeKey, error);
        if (carrier == nullptr || abiType == nullptr) return nullptr;
        // EN: C3-02. An `iN` result extends to its carrier by the carrier's signedness, as C converts `_BitInt(N)`.
        // ES: C3-02. Un resultado `iN` se extiende a su portador segun el signo del portador, como C convierte.
        if (LLVMGetTypeKind(carrier) == LLVMIntegerTypeKind && LLVMGetTypeKind(abiType) == LLVMIntegerTypeKind &&
            LLVMGetIntTypeWidth(abiType) < LLVMGetIntTypeWidth(carrier))
        {
            const bool signedCarrier = typeKey == "builtin:sbyte" || typeKey == "builtin:short" ||
                typeKey == "builtin:int" || typeKey == "builtin:long" || typeKey == "builtin:i128";
            return signedCarrier ? LLVMBuildSExt(builder_, value, carrier, "c.bitint.result")
                                 : LLVMBuildZExt(builder_, value, carrier, "c.bitint.result");
        }
        const unsigned alignment = vectorSlotAlignment(typeKey);
        LLVMValueRef slot = entryBlockAlloca(abiType, alignment, "c.vector.abi.to.carrier");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        LLVMSetAlignment(store, alignment);
        LLVMValueRef load = LLVMBuildLoad2(builder_, carrier, slot, "c.vector.carrier.value");
        LLVMSetAlignment(load, alignment);
        return load;
    }

LLVMValueRef OopLlvmImplementation::vectorCallAbiToCarrier(
        LLVMValueRef value, std::string_view typeKey, std::string& error)
{
        LLVMTypeRef carrier = llvmType(typeKey, error);
        LLVMTypeRef abiType = vectorCallAbiType(typeKey);
        if (carrier == nullptr || abiType == nullptr) return nullptr;
        const unsigned alignment = vectorSlotAlignment(typeKey);
        LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "vectorcall.abi.to.carrier");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        LLVMSetAlignment(store, alignment);
        LLVMValueRef load = LLVMBuildLoad2(builder_, carrier, slot, "vectorcall.carrier.value");
        LLVMSetAlignment(load, alignment);
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

// Keep the backing allocation at the native stack alignment on Win64. The
// rounded address still satisfies the imported C++ layout, including in a
// function with exception funclets, where LLVM cannot realign the frame.
LLVMValueRef OopLlvmImplementation::entryBlockCppStorage(
    LLVMTypeRef type, std::uint64_t size, unsigned alignment, const char* name)
{
    if (alignment <= 16U || (alignment & (alignment - 1U)) != 0U ||
        size > std::numeric_limits<std::uint64_t>::max() - (alignment - 1U))
        return entryBlockAlloca(type, alignment, name);
    LLVMBasicBlockRef saved = LLVMGetInsertBlock(builder_);
    LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(currentFunction_);
    LLVMValueRef first = LLVMGetFirstInstruction(entry);
    if (first != nullptr) LLVMPositionBuilderBefore(builder_, first);
    else enterBlock(entry);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
    LLVMTypeRef bufferType = LLVMArrayType2(LLVMInt8TypeInContext(context_), size + alignment - 1U);
    LLVMValueRef buffer = LLVMBuildAlloca(builder_, bufferType, name);
    LLVMSetAlignment(buffer, 16U);
    LLVMValueRef address = LLVMBuildPtrToInt(builder_, buffer, i64, "cpp.align.address");
    LLVMValueRef rounded = LLVMBuildAdd(builder_, address, LLVMConstInt(i64, alignment - 1U, 0), "cpp.align.add");
    LLVMValueRef masked = LLVMBuildAnd(builder_, rounded, LLVMConstInt(i64, ~std::uint64_t(alignment - 1U), 0), "cpp.align.mask");
    LLVMValueRef result = LLVMBuildIntToPtr(builder_, masked, LLVMPointerTypeInContext(context_, 0), "cpp.aligned.storage");
    enterBlock(saved);
    return result;
}

LLVMValueRef OopLlvmImplementation::entryBlockAbiStorage(
    LLVMTypeRef type, std::string_view typeKey, const char* name)
{
    const auto ordinal = typeKey.starts_with("type:")
        ? nameLookup_.typeOrdinalForIdentity(typeKey.substr(5)) : std::nullopt;
    const auto* cppType = ordinal ? nameLookup_.cppImportedType(*ordinal) : nullptr;
    if (cppType != nullptr && cppType->objectLayout && cppType->objectLayout->sizeBytes != 0 &&
        cppType->objectLayout->alignmentBytes <= std::numeric_limits<unsigned>::max())
        return entryBlockCppStorage(type, cppType->objectLayout->sizeBytes,
            static_cast<unsigned>(cppType->objectLayout->alignmentBytes), name);
    return entryBlockAlloca(type, structAlignmentFor(typeKey), name);
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
        LLVMValueRef copy = entryBlockAbiStorage(carrier, typeKey, "abi.arg.copy");
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
