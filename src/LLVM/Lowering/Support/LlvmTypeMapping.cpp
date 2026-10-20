#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/DebugMetadataVersion.h"

#include "../../LlvmCApi.h"
#include "../../SignedDivisionCorrection.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"
#include "nativesharp/LLVM/LlvmDebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmModuleMetadata.h"
#include "nativesharp/LLVM/LlvmRuntimeNames.h"
#include "nativesharp/LLVM/LlvmTypeClassification.h"
#include "nativesharp/LLVM/LlvmTypeLayout.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool isFloating(std::string_view type) noexcept
{
    return type == "builtin:float" || type == "builtin:double";
}

// NSC305-L03. Maps the builtin type keys to their LLVM carriers (moved out of the lowerer's llvmType
// as a pure move). Returns nullptr for a non-builtin key.
LLVMTypeRef mapBuiltinLlvmType(LLVMContextRef context, std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:void") return LLVMVoidTypeInContext(context);
    if (typeKey == "builtin:bool") return LLVMInt1TypeInContext(context);
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte" ||
        typeKey == "builtin:char") return LLVMInt8TypeInContext(context);
    if (typeKey == "builtin:short" || typeKey == "builtin:ushort")
        return LLVMInt16TypeInContext(context);
    if (typeKey == "builtin:int" || typeKey == "builtin:uint")
        return LLVMInt32TypeInContext(context);
    if (typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint")
        return LLVMInt64TypeInContext(context);
    if (typeKey == "builtin:i128" || typeKey == "builtin:u128")
        return LLVMIntTypeInContext(context, 128);
    // NS304-T15-F01: decimal is a 128-bit value (96-bit coefficient + scale + sign).
    if (typeKey == "builtin:decimal")
        return LLVMIntTypeInContext(context, 128);
    if (typeKey == "builtin:float") return LLVMFloatTypeInContext(context);
    if (typeKey == "builtin:double") return LLVMDoubleTypeInContext(context);
    return nullptr;
}

// NSC305-L03. Maps the pointer-like type keys (ref/refmut/string/native-array/reference) to `ptr`
// (moved out of the lowerer's llvmType as a pure move). Returns nullptr for other keys.
LLVMTypeRef mapPointerLikeLlvmType(
    LLVMContextRef context,
    const frontend::ProjectNameLookup& nameLookup,
    std::string_view typeKey) noexcept
{
    if (typeKey.starts_with("ref:") || typeKey.starts_with("refmut:"))
        return LLVMPointerTypeInContext(context, 0);
    if (typeKey == "builtin:null" || typeKey == "string-literal" ||
        typeKey == "builtin:string" ||
        frontend::ProjectTypeConversion::isNativeArray(typeKey) ||
        isPointerLikeType(typeKey) || isClassReferenceType(nameLookup, typeKey) ||
        isDelegateReferenceType(nameLookup, typeKey))
        return LLVMPointerTypeInContext(context, 0);
    return nullptr;
}

LLVMTypeRef mapNonBuiltinLlvmType(
    LLVMContextRef context,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectOopMirModel& mirModel,
    std::string_view typeKey,
    std::string& error)
{
    if (const auto simd = language::parsePortableSimdType(typeKey); simd.has_value())
    {
        LLVMTypeRef lane = simd->mask
            ? LLVMInt1TypeInContext(context)
            : mapLlvmType(context, nameLookup, mirModel, simd->laneTypeKey, error);
        return lane == nullptr
            ? nullptr
            : LLVMVectorType(lane, static_cast<unsigned>(simd->laneCount));
    }
    if (language::isAnonymousLambdaTypeIdentity(typeKey))
        return LLVMStructTypeInContext(context, nullptr, 0, 0);
    // C2.1 "fixed arrays": `T[N]` is INLINE storage -- its LLVM carrier is exactly `[N x T]`.
    if (const std::optional<std::string> element =
            frontend::ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
        element.has_value())
    {
        const std::optional<std::uint64_t> count =
            frontend::ProjectMemberModel::fixedArrayCount(typeKey);
        LLVMTypeRef elementType = mapLlvmType(context, nameLookup, mirModel, *element, error);
        if (elementType == nullptr || !count.has_value() || *count == 0)
        {
            if (error.empty())
                error = "C2.1 cannot map fixed-array type key to LLVM type: " +
                    std::string(typeKey);
            return nullptr;
        }
        return LLVMArrayType2(elementType, static_cast<unsigned>(*count));
    }
    if (typeKey.starts_with("type:") || typeKey.starts_with("future:") ||
        typeKey.starts_with("iterator:") || typeKey.starts_with("async-iterator:") ||
        frontend::ProjectTypeConversion::isTuple(typeKey))
    {
        if (const frontend::builtins::IntegerTypeInfo* enumInfo =
                findEnumUnderlyingInfo(nameLookup, typeKey);
            enumInfo != nullptr)
        {
            switch (enumInfo->width)
            {
                case 8: return LLVMInt8TypeInContext(context);
                case 16: return LLVMInt16TypeInContext(context);
                case 64: return LLVMInt64TypeInContext(context);
                default: return LLVMInt32TypeInContext(context);
            }
        }
        if (const frontend::ProjectOopMirStructLayout* layout =
                findStructLayout(mirModel, typeKey);
            layout != nullptr)
        {
            switch (layout->size)
            {
                case 1: return LLVMInt8TypeInContext(context);
                case 2: return LLVMInt16TypeInContext(context);
                case 4: return LLVMInt32TypeInContext(context);
                case 8: return LLVMInt64TypeInContext(context);
                default:
                    return LLVMArrayType2(LLVMInt8TypeInContext(context), layout->size);
            }
        }
        error = "Stage 40 cannot map named value type to LLVM carrier: " + std::string(typeKey);
        return nullptr;
    }
    error = "Stage 34 cannot map type key to LLVM type: " + std::string(typeKey);
    return nullptr;
}

// NSC305-L03. The full semantic-type -> LLVM mapping (moved out of the lowerer's llvmType as a pure
// move). `mapNonBuiltinLlvmType` covers simd/fixed-array/named and recurses through `mapLlvmType`.
LLVMTypeRef mapLlvmType(
    LLVMContextRef context,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectOopMirModel& mirModel,
    std::string_view typeKey,
    std::string& error)
{
    if (LLVMTypeRef builtin = mapBuiltinLlvmType(context, typeKey))
        return builtin;
    if (LLVMTypeRef pointerLike = mapPointerLikeLlvmType(context, nameLookup, typeKey))
        return pointerLike;
    return mapNonBuiltinLlvmType(context, nameLookup, mirModel, typeKey, error);
}

}
