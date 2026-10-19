#pragma once

#include "nativesharp/Language/FfiOwnership.h"

#include <cstddef>
#include <optional>

namespace nativesharp::language
{
struct FfiParameterContract final
{
    bool nullable = false;
    FfiOwnership ownership = FfiOwnership::Borrowed;
    std::optional<std::size_t> lengthParameter;
    bool callback = false;
    bool callbackMayEscape = false;
};
}
