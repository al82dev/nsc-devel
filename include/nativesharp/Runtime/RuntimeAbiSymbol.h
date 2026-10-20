#pragma once

#include "nativesharp/Runtime/RuntimeAbiSymbolKind.h"

#include <string_view>

namespace nativesharp::runtime
{
struct RuntimeAbiSymbol
{
    std::string_view name;
    RuntimeAbiSymbolKind kind;
    // Canonical opaque-pointer LLVM type. Parameter names and implementation
    // details are deliberately excluded: this is the binary boundary.
    std::string_view llvmType;
};
}
