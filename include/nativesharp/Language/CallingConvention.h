#pragma once

namespace nativesharp::language
{
// T19-028: Swift, SwiftAsync and IntelOclBicc are appended; earlier values keep their order. / Se agregan al final.
enum class CallingConvention
{
    NativeSharp, C, StdCall, FastCall, VectorCall, Win64, SysV, RegCall, PreserveMost, PreserveAll,
    PreserveNone, Swift, SwiftAsync, IntelOclBicc
};
}
