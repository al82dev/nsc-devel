#pragma once

#include <cstddef>
#include <optional>

namespace nativesharp::language::detail
{
std::optional<std::size_t> checkedLayoutAdd(
    std::size_t left,
    std::size_t right) noexcept;

std::optional<std::size_t> checkedLayoutAlignUp(
    std::size_t value,
    std::size_t alignment) noexcept;
}
