#pragma once

#include "nativesharp/Frontend/ImplicitDefaultConstructorResolutionKind.h"

#include <cstddef>
#include <optional>

namespace nativesharp::frontend
{
struct ImplicitDefaultConstructorBinding final
{
    ImplicitDefaultConstructorResolutionKind resolution =
        ImplicitDefaultConstructorResolutionKind::Invalid;
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::optional<std::size_t> directBaseConstructorMemberOrdinal;
    std::optional<std::size_t> entryConstructorMemberOrdinal;
};
}
