#include "nativesharp/Sema/SymbolKind.h"

namespace nativesharp::sema
{
std::string_view symbolKindName(SymbolKind kind) noexcept
{
    switch (kind)
    {
#define NS_SYMBOL_KIND(name) case SymbolKind::name: return #name
        NS_SYMBOL_KIND(Variable);
        NS_SYMBOL_KIND(GlobalVariable);
        NS_SYMBOL_KIND(Parameter);
        NS_SYMBOL_KIND(Function);
        NS_SYMBOL_KIND(Struct);
        NS_SYMBOL_KIND(Enum);
        NS_SYMBOL_KIND(Field);
        NS_SYMBOL_KIND(EnumMember);
#undef NS_SYMBOL_KIND
    }
    return "Symbol";
}
}
