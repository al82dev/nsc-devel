#include "nativesharp/Sema/TypeKind.h"

namespace nativesharp::sema
{
std::string_view typeKindName(TypeKind kind) noexcept
{
    switch (kind)
    {
#define NS_TYPE_KIND(name, text) case TypeKind::name: return text
        NS_TYPE_KIND(Error, "<error>");
        NS_TYPE_KIND(Void, "void");
        NS_TYPE_KIND(Bool, "bool");
        NS_TYPE_KIND(Byte, "byte");
        NS_TYPE_KIND(Char, "char");
        NS_TYPE_KIND(Short, "short");
        NS_TYPE_KIND(UShort, "ushort");
        NS_TYPE_KIND(Int, "int");
        NS_TYPE_KIND(UInt, "uint");
        NS_TYPE_KIND(Long, "long");
        NS_TYPE_KIND(ULong, "ulong");
        NS_TYPE_KIND(NInt, "nint");
        NS_TYPE_KIND(NUInt, "nuint");
        NS_TYPE_KIND(Float, "float");
        NS_TYPE_KIND(Double, "double");
        NS_TYPE_KIND(Pointer, "pointer");
        NS_TYPE_KIND(Array, "array");
        NS_TYPE_KIND(Struct, "struct");
        NS_TYPE_KIND(Enum, "enum");
        NS_TYPE_KIND(Function, "function");
        NS_TYPE_KIND(NullLiteral, "null");
        NS_TYPE_KIND(StringLiteral, "string-literal");
#undef NS_TYPE_KIND
    }
    return "<error>";
}
}
