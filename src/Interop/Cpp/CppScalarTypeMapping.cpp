#include "nativesharp/Interop/CppScalarTypeMapping.h"

#include <string_view>

namespace nativesharp::interop
{
std::optional<CppScalarTypeMapping> mapCppScalarType(const CppType& type)
{
    if (type.canonicalSpelling.empty()) return std::nullopt;
    // EN: C3-02. A `_BitInt(N)` with 64 < N < 128 travels in memory in Clang's Windows x64 ABI, unlike the 128-bit
    //     carrier: it stays unmapped. N <= 64 and N == 128 use the integer carriers of their size.
    // ES: C3-02. Un `_BitInt(N)` con 64 < N < 128 viaja en memoria en el ABI de Clang para Windows x64, a diferencia
    //     del portador de 128 bits: queda sin mapear. N <= 64 y N == 128 usan los portadores enteros de su tamano.
    if (type.bitIntWidth && *type.bitIntWidth > 64U && *type.bitIntWidth < 128U) return std::nullopt;
    std::string_view key;
    // C3-03: an enum over a 128-bit integer is carried as that integer. / Un enum sobre un entero de 128 bits se
    //     lleva como ese entero.
    if (type.kind == CppTypeKind::Enumeration && type.enumUnderlying && type.sizeBits == 128U)
    {
        auto underlying = mapCppScalarType(*type.enumUnderlying);
        if (!underlying) return std::nullopt;
        underlying->canonicalCppType = type.canonicalSpelling;
        underlying->isConst = type.isConst;
        underlying->isVolatile = type.isVolatile;
        return underlying;
    }
    if (type.kind == CppTypeKind::Void) key = "builtin:void";
    else if (type.kind == CppTypeKind::Boolean && type.sizeBits == 8) key = "builtin:bool";
    else if (type.kind == CppTypeKind::SignedInteger)
    {
        if (type.sizeBits == 8) key = "builtin:sbyte";
        else if (type.sizeBits == 16) key = "builtin:short";
        else if (type.sizeBits == 32) key = "builtin:int";
        else if (type.sizeBits == 64) key = "builtin:long";
        // T19-004: `__int128`. / `__int128`.
        else if (type.sizeBits == 128) key = "builtin:i128";
    }
    else if (type.kind == CppTypeKind::UnsignedInteger)
    {
        if (type.sizeBits == 8) key = "builtin:byte";
        else if (type.sizeBits == 16) key = "builtin:ushort";
        else if (type.sizeBits == 32) key = "builtin:uint";
        else if (type.sizeBits == 64) key = "builtin:ulong";
        // T19-004: `unsigned __int128`. / `unsigned __int128`.
        else if (type.sizeBits == 128) key = "builtin:u128";
    }
    else if (type.kind == CppTypeKind::Floating)
    {
        if (type.unqualifiedCanonicalSpelling == "float" && type.sizeBits == 32) key = "builtin:float";
        else if (type.sizeBits == 64 && (type.unqualifiedCanonicalSpelling == "double" ||
                 (type.unqualifiedCanonicalSpelling == "long double" && type.ieeeBinary64))) key = "builtin:double";
        // EN: C3-01. `_Float16` is IEEE binary16, the storage of NativeSharp.Core.Numerics.Half (`ushort Bits`):
        //     fields, pointers and globals share it; by value, a C import passes it as Clang does (XMM, LLVM `half`).
        // ES: C3-01. `_Float16` es IEEE binary16, el almacenamiento de NativeSharp.Core.Numerics.Half: campos,
        //     punteros y globales lo comparten; por valor, una importacion C lo pasa como Clang (XMM, `half` de LLVM).
        else if (type.sizeBits == 16 && type.unqualifiedCanonicalSpelling == "_Float16")
            key = kCppHalfTypeKey;
    }
    if (key.empty()) return std::nullopt;
    return CppScalarTypeMapping{std::string(key), type.canonicalSpelling, type.isConst, type.isVolatile};
}
}
