#pragma once

#include "nativesharp/Language/CompatibilitySemantics.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
enum class DeprecationKind { SyntaxAlias, Behavior };
enum class DeprecationState { Suppressed, Warning, Error, Removed };

struct DeprecationRelease final
{
    unsigned major = 3;
    unsigned minor = 0;
};

struct DeprecatedFeature final
{
    std::string_view identity;
    DeprecationKind kind = DeprecationKind::Behavior;
    language::MigrationIssue issue = language::MigrationIssue::NewlyContextualWord;
    DeprecationRelease deprecatedSince;
    DeprecationRelease removalNotBefore{4, 0};
};

struct DeprecationDecision final
{
    const DeprecatedFeature* feature = nullptr;
    DeprecationState state = DeprecationState::Warning;
};

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
