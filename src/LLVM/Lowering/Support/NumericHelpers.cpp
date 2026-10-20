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

// NS304-T15-F02: derive the decimal coefficient and scale from a binary FP literal value. The
// `%g` round-trip gives the shortest decimal text that round-trips, so common literals such as
// `1.5` yield coefficient 15 / scale 1 and `0.1` yields coefficient 1 / scale 1. The coefficient is
// limited to 64 bits here; wider coefficients are validated in F05 (overflow) with the exact digits.
bool decimalCoefficientAndScale(double value, std::uint64_t& coefficient, int& scale) noexcept
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    std::string text(buffer);
    const auto e = text.find_first_of("eE");
    std::string mantissa = e == std::string::npos ? text : text.substr(0, e);
    int exponent = 0;
    if (e != std::string::npos)
    {
        const char* start = text.data() + e + 1;
        const auto parsed = std::from_chars(start, text.data() + text.size(), exponent);
        if (parsed.ec != std::errc{}) return false;
    }
    const auto dot = mantissa.find('.');
    std::string digits = dot == std::string::npos
        ? mantissa
        : mantissa.substr(0, dot) + mantissa.substr(dot + 1);
    int baseScale = dot == std::string::npos ? 0 : static_cast<int>(mantissa.size() - dot - 1);
    const auto first = digits.find_first_not_of('0');
    if (first == std::string::npos)
    {
        coefficient = 0;
        scale = 0;
        return true;
    }
    digits = digits.substr(first);
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), coefficient);
    if (parsed.ec != std::errc{}) return false;
    int effectiveScale = baseScale - exponent;
    if (effectiveScale < 0)
    {
        for (int i = 0; i < -effectiveScale; ++i)
        {
            if (coefficient > (std::numeric_limits<std::uint64_t>::max() / 10U)) return false;
            coefficient *= 10U;
        }
        effectiveScale = 0;
    }
    scale = effectiveScale;
    return true;
}

LLVMValueRef applyFloatingPointMode(LLVMValueRef instruction, bool contractable) noexcept
{
    if (instruction == nullptr) return instruction;
    unsigned flags = activeFloatingPointMode() == FloatingPointMode::Fast
        ? static_cast<unsigned>(LLVMFastMathFast)
        : static_cast<unsigned>(LLVMFastMathNone);
    if (activeFmaContractPolicy() == FmaContractPolicy::Off)
        flags &= ~static_cast<unsigned>(LLVMFastMathAllowContract);
    else if (contractable)
        flags |= static_cast<unsigned>(LLVMFastMathAllowContract);
    if (flags != 0U && LLVMCanValueUseFastMathFlags(instruction) != 0)
        LLVMSetFastMathFlags(instruction, static_cast<LLVMFastMathFlags>(flags));
    return instruction;
}

FloatingPointState sourceFloatingPointState(
    ast::FloatingPointModeSyntaxKind mode,
    FloatingPointState inherited) noexcept
{
    inherited.denormalMode = FloatingPointDenormalMode::Preserve;
    switch (mode)
    {
        case ast::FloatingPointModeSyntaxKind::Inherit:
            return inherited;
        case ast::FloatingPointModeSyntaxKind::Strict:
            inherited.mode = FloatingPointMode::Strict;
            inherited.fmaContractPolicy = FmaContractPolicy::Off;
            inherited.roundingMode = FloatingPointRoundingMode::NearestEven;
            return inherited;
        case ast::FloatingPointModeSyntaxKind::Precise:
            inherited.mode = FloatingPointMode::Precise;
            inherited.fmaContractPolicy = FmaContractPolicy::On;
            inherited.roundingMode = FloatingPointRoundingMode::Dynamic;
            return inherited;
        case ast::FloatingPointModeSyntaxKind::Fast:
            inherited.mode = FloatingPointMode::Fast;
            inherited.fmaContractPolicy = FmaContractPolicy::Fast;
            inherited.roundingMode = FloatingPointRoundingMode::Dynamic;
            return inherited;
    }
    return inherited;
}

// LA TERCERA Y LA CUARTA COPIA de la tabla de tipos enteros, encontradas al arreglar la division
// el 2026-09-03 y quitadas en el mismo sitio. Estas dos funciones repetian los anchos y los
// signos que ya estaban en include/nativesharp/Frontend/BuiltinIntegerTypes.h -- y esta copia era
// la mas peligrosa de todas, porque es la que decide sdiv contra udiv, ashr contra lshr y sext
// contra zext. Un desacuerdo aqui no da un error: da CODIGO GENERADO QUE HACE OTRA COSA.

// ID 89 sigue siendo el motivo de que `sbyte` este en la tabla como el unico entero de 8 bits con
// signo; ahora esa decision se lee de un solo sitio en vez de estar escrita cuatro veces.
bool isSignedInteger(std::string_view type) noexcept
{
    const frontend::builtins::IntegerTypeInfo* info = frontend::builtins::findIntegerType(type);
    return info != nullptr && info->isSigned;
}

unsigned integerWidth(std::string_view type) noexcept
{
    // `bool` no es un entero incorporado a efectos de promocion ni de plegado -- la tabla lo deja
    // fuera a proposito y lo fija con un static_assert-- pero SI tiene un ancho LLVM: i1.
    if (type == "builtin:bool") return 1;
    const frontend::builtins::IntegerTypeInfo* info = frontend::builtins::findIntegerType(type);
    return info != nullptr ? info->width : 0;
}

bool isComparison(lexer::TokenKind kind) noexcept
{
    return kind == lexer::TokenKind::EqualEqual || kind == lexer::TokenKind::BangEqual ||
        kind == lexer::TokenKind::Less || kind == lexer::TokenKind::LessEqual ||
        kind == lexer::TokenKind::Greater || kind == lexer::TokenKind::GreaterEqual;
}

LLVMIntPredicate integerPredicate(lexer::TokenKind kind, bool signedOperands)
{
    switch (kind)
    {
        case lexer::TokenKind::EqualEqual: return LLVMIntEQ;
        case lexer::TokenKind::BangEqual: return LLVMIntNE;
        case lexer::TokenKind::Less: return signedOperands ? LLVMIntSLT : LLVMIntULT;
        case lexer::TokenKind::LessEqual: return signedOperands ? LLVMIntSLE : LLVMIntULE;
        case lexer::TokenKind::Greater: return signedOperands ? LLVMIntSGT : LLVMIntUGT;
        case lexer::TokenKind::GreaterEqual: return signedOperands ? LLVMIntSGE : LLVMIntUGE;
        default: return LLVMIntEQ;
    }
}

LLVMRealPredicate floatingPredicate(lexer::TokenKind kind)
{
    switch (kind)
    {
        case lexer::TokenKind::EqualEqual: return LLVMRealOEQ;
        case lexer::TokenKind::BangEqual: return LLVMRealUNE;
        case lexer::TokenKind::Less: return LLVMRealOLT;
        case lexer::TokenKind::LessEqual: return LLVMRealOLE;
        case lexer::TokenKind::Greater: return LLVMRealOGT;
        case lexer::TokenKind::GreaterEqual: return LLVMRealOGE;
        default: return LLVMRealOEQ;
    }
}

}
