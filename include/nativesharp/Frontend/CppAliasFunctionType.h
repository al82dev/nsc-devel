#pragma once

#include "nativesharp/AST/FunctionCallingConventionSyntaxKind.h"
#include "nativesharp/Frontend/CppAliasBuiltinType.h"

namespace nativesharp::frontend
{
struct CppAliasFunctionType
{
    CppAliasBuiltinType result;
    std::vector<CppAliasBuiltinType> parameters;
    ast::FunctionCallingConventionSyntaxKind callingConvention;
    bool variadic;
};

// Preserve the complete captured prototype; unrepresentable signatures stay rejected.
inline std::optional<CppAliasFunctionType> cppAliasFunctionType(const interop::CppType& type)
{
    using K = interop::CppTypeKind;
    using C = ast::FunctionCallingConventionSyntaxKind;
    if (type.kind != K::Pointer || type.isConst || type.isVolatile || type.sizeBits != 64 ||
        !type.element || type.element->kind != K::Function || !type.element->function)
        return std::nullopt;
    const auto& signature = *type.element->function;
    if (!signature.hasPrototype) return std::nullopt;
    C convention;
    if (signature.callingConvention == "cdecl") convention = C::C;
    else if (signature.callingConvention == "stdcall") convention = C::StdCall;
    else if (signature.callingConvention == "fastcall") convention = C::FastCall;
    else if (signature.callingConvention == "vectorcall") convention = C::VectorCall;
    else if (signature.callingConvention == "ms_abi") convention = C::Win64;
    else return std::nullopt;
    const auto result = cppAliasBuiltinType(signature.returnType);
    if (!result) return std::nullopt;
    CppAliasFunctionType mapped{*result, {}, convention, signature.variadic};
    for (const auto& parameter : signature.parameterTypes)
    {
        const auto value = cppAliasBuiltinType(parameter);
        if (!value || (value->token == lexer::TokenKind::KwVoid && value->pointerDepth == 0))
            return std::nullopt;
        mapped.parameters.push_back(*value);
    }
    return mapped;
}
}
