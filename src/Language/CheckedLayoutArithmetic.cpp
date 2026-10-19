#include "CheckedLayoutArithmetic.h"

#include <limits>

namespace nativesharp::language::detail
{
std::optional<std::size_t> checkedLayoutAdd(
    std::size_t left,
    std::size_t right) noexcept
{
    if (left > std::numeric_limits<std::size_t>::max() - right)
    {
        return std::nullopt;
    }
    return left + right;
}

std::optional<std::size_t> checkedLayoutAlignUp(
    std::size_t value,
    std::size_t alignment) noexcept
{
    if (alignment == 0)
    {
        return std::nullopt;
    }

    const std::optional<std::size_t> adjusted = checkedLayoutAdd(value, alignment - 1U);
    if (!adjusted.has_value())
    {
        return std::nullopt;
    }
    return (*adjusted / alignment) * alignment;
}
}
