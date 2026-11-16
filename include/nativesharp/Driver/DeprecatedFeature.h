#pragma once

#include "nativesharp/Driver/DeprecationKind.h"
#include "nativesharp/Language/CompatibilitySemantics.h"

#include <string_view>

namespace nativesharp::driver
{
struct DeprecatedFeature final
{
    std::string_view identity;
    DeprecationKind kind = DeprecationKind::Behavior;
    language::MigrationIssue issue = language::MigrationIssue::NewlyContextualWord;
    bool removed = false;
};
} // namespace nativesharp::driver
