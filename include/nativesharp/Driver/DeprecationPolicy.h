#pragma once

#include "nativesharp/Driver/DeprecatedFeature.h"
#include "nativesharp/Driver/DeprecationDecision.h"
#include "nativesharp/Driver/DeprecationKind.h"
#include "nativesharp/Driver/DeprecationState.h"
#include "nativesharp/Language/CompatibilitySemantics.h"

#include <span>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
std::span<const DeprecatedFeature> deprecatedFeatures() noexcept;
const DeprecatedFeature* deprecatedFeature(std::string_view identity) noexcept;
const DeprecatedFeature* deprecatedFeature(language::MigrationIssue issue) noexcept;

DeprecationDecision evaluateDeprecation(
    const DeprecatedFeature& feature,
    bool suppressed,
    bool warningsAsErrors) noexcept;

bool deprecationRemovalAllowed(
    const DeprecatedFeature& feature,
    bool previouslyDeprecated,
    bool documentedSecurityOrUndefinedBehaviorFix) noexcept;

std::string_view deprecationStateName(DeprecationState state) noexcept;
std::string currentDeprecationPolicy();
}
