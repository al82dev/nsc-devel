#pragma once

#include <string_view>

namespace nativesharp::llvm_backend
{
enum class DebugInformationKind
{
    None,
    LineTablesOnly,
    Full,
};

std::string_view debugInformationKindName(DebugInformationKind kind) noexcept;
}
