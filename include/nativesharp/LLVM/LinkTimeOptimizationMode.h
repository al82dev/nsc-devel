#pragma once

#include <string_view>

namespace nativesharp::llvm_backend
{
enum class LinkTimeOptimizationMode
{
    None,
    Full,
    Thin
};

std::string_view linkTimeOptimizationModeName(LinkTimeOptimizationMode mode) noexcept;
}
