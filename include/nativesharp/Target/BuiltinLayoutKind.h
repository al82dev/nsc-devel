#pragma once

namespace nativesharp::target
{
enum class BuiltinLayoutKind
{
    Bool,
    // ID 89. `sbyte` mide lo mismo que `byte` -- un byte -- y se distingue de el en el SIGNO,
    // que esta capa no modela: aqui solo viven tamano y alineacion. La distincion la hacen las
    // tablas de signo del frontend, del MIR y del backend.
    SByte,
    Byte,
    Char,
    Short,
    UShort,
    Int,
    UInt,
    Long,
    ULong,
    NInt,
    NUInt,
    Int128,
    UInt128,
    Float,
    Double,
    Enum,
    Pointer,
    // NS304-T15-F01: 128-bit decimal (96-bit coefficient + scale + sign) is 16 bytes.
    // Keep additions at the end: these ordinal identities are externally observed.
    Decimal
};
}
