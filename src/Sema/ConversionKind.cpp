#include "nativesharp/Sema/ConversionKind.h"

namespace nativesharp::sema
{
std::string_view ToString(ConversionKind kind) noexcept
{
    switch (kind)
    {
        case ConversionKind::Identity:
            return "identity";
        case ConversionKind::IntegralPromotion:
            return "integral-promotion";
        case ConversionKind::ImplicitConstantInteger:
            return "implicit-constant-integer";
        case ConversionKind::ImplicitNumeric:
            return "implicit-numeric";
        case ConversionKind::ImplicitNullPointer:
            return "implicit-null-pointer";
        case ConversionKind::ImplicitPointerToVoid:
            return "implicit-pointer-to-void";
        case ConversionKind::ImplicitStringLiteralToCharPointer:
            return "implicit-string-literal-to-char-pointer";
        case ConversionKind::ExplicitNumeric:
            return "explicit-numeric";
        case ConversionKind::ExplicitEnum:
            return "explicit-enum";
        case ConversionKind::ExplicitPointer:
            return "explicit-pointer";
        case ConversionKind::ExplicitPointerInteger:
            return "explicit-pointer-integer";
        case ConversionKind::ExplicitNullPointer:
            return "explicit-null-pointer";
        case ConversionKind::ToVoid:
            return "to-void";
    }
    return "unknown";
}
}
