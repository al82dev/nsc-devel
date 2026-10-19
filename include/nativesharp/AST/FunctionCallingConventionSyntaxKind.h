#pragma once

namespace nativesharp::ast
{
enum class FunctionCallingConventionSyntaxKind
{
    NativeSharp,
    C,
    StdCall,
    FastCall,
    VectorCall,
    Win64
};
}
