#pragma once

#include "nativesharp/Basic/TransparentStringHash.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <span>
#include <unordered_set>

namespace nativesharp::parser
{
using KnownTypeNameHash = basic::TransparentStringHash;

using KnownTypeNameSet = std::unordered_set<
    std::string,
    KnownTypeNameHash,
    std::equal_to<>>;

KnownTypeNameSet buildKnownTypeNameSet(
    std::span<const std::string> knownTypeNames = {});
}
