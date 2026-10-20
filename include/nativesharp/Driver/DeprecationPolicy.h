#pragma once

#include "nativesharp/Driver/DeprecatedFeature.h"
#include "nativesharp/Driver/DeprecationDecision.h"
#include "nativesharp/Driver/DeprecationKind.h"
#include "nativesharp/Driver/DeprecationRelease.h"
#include "nativesharp/Driver/DeprecationState.h"
#include "nativesharp/Language/CompatibilitySemantics.h"

#include <optional>
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
    DeprecationRelease compilerRelease,
    bool suppressed,
    bool warningsAsErrors) noexcept;

bool deprecationRemovalAllowed(
    const DeprecatedFeature& feature,
    DeprecationRelease release,
    bool previouslyDeprecated,
    bool documentedSecurityOrUndefinedBehaviorFix) noexcept;

std::optional<DeprecationRelease> parseDeprecationRelease(std::string_view text) noexcept;
std::string_view deprecationStateName(DeprecationState state) noexcept;
std::string currentDeprecationPolicy();
}
