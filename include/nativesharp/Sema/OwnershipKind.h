#pragma once

#include <string_view>

namespace nativesharp::sema
{
enum class OwnershipKind
{
    None,
    Owned,
    Borrowed
};

std::string_view ownershipKindName(OwnershipKind kind) noexcept;
}
