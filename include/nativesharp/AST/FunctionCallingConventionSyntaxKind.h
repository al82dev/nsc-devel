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
    Win64,
    SysV,
    RegCall,
    PreserveMost,
    PreserveAll,
    PreserveNone,
    // T19-028: the C conventions swiftcall, swiftasynccall and intel_ocl_bicc. / Las convenciones C swiftcall,
    //     swiftasynccall e intel_ocl_bicc.
    Swift,
    SwiftAsync,
    IntelOclBicc
};
}
