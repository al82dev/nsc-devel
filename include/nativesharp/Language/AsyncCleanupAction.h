#pragma once

#include "nativesharp/Language/AsyncCleanupKind.h"

#include <cstddef>
#include <optional>
#include <string>

namespace nativesharp::language
{
struct AsyncCleanupAction final
{
    AsyncCleanupKind kind = AsyncCleanupKind::LocalDrop;
    std::string name;
    std::optional<std::size_t> frameSlot;
};
}
