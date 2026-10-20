#pragma once

#include "nativesharp/Driver/DeprecationKind.h"
#include "nativesharp/Driver/DeprecationRelease.h"
#include "nativesharp/Language/CompatibilitySemantics.h"

#include <string_view>

namespace nativesharp::driver
{
struct DeprecatedFeature final
{
    std::string_view identity;
    DeprecationKind kind = DeprecationKind::Behavior;
    language::MigrationIssue issue = language::MigrationIssue::NewlyContextualWord;
    DeprecationRelease deprecatedSince;
    DeprecationRelease removalNotBefore{4, 0};
};
} // namespace nativesharp::driver
