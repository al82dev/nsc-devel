#pragma once

#include <string_view>

namespace nativesharp::sema
{
enum class SymbolKind
{
    Variable,
    GlobalVariable,
    Parameter,
    Function,
    Struct,
    Enum,
    Field,
    EnumMember
};

std::string_view symbolKindName(SymbolKind kind) noexcept;
}
