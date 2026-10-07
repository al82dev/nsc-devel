#include "nativesharp/Driver/DeprecationPolicy.h"

#include <array>
#include <charconv>
#include <sstream>

namespace nativesharp::driver
{
namespace
{
constexpr std::array<DeprecatedFeature, 7> kFeatures = {{
    {"contextual-identifier-alias", DeprecationKind::SyntaxAlias,
        language::MigrationIssue::NewlyContextualWord, {3, 0}, {4, 0}},
    {"legacy-null-overload", DeprecationKind::Behavior,
        language::MigrationIssue::OverloadResolution, {3, 0}, {4, 0}},
    {"implicit-owned-copy", DeprecationKind::Behavior,
        language::MigrationIssue::CopyToMove, {3, 0}, {4, 0}},
    {"implicit-null-pointer", DeprecationKind::Behavior,
        language::MigrationIssue::Nullability, {3, 0}, {4, 0}},
    {"using-import-alias", DeprecationKind::SyntaxAlias,
        language::MigrationIssue::ImportLookup, {3, 0}, {4, 0}},
    {"implicit-numeric-narrowing", DeprecationKind::Behavior,
        language::MigrationIssue::NumericNarrowing, {3, 0}, {4, 0}},
    {"legacy-cleanup-order", DeprecationKind::Behavior,
        language::MigrationIssue::CleanupOrder, {3, 0}, {4, 0}},
}};

bool releaseAtLeast(DeprecationRelease value, DeprecationRelease required) noexcept
{
    return value.major > required.major ||
        (value.major == required.major && value.minor >= required.minor);
}
}

std::span<const DeprecatedFeature> deprecatedFeatures() noexcept
{
    return kFeatures;
}

const DeprecatedFeature* deprecatedFeature(std::string_view identity) noexcept
{
    for (const DeprecatedFeature& feature : kFeatures)
        if (feature.identity == identity) return &feature;
    return nullptr;
}

const DeprecatedFeature* deprecatedFeature(language::MigrationIssue issue) noexcept
{
    for (const DeprecatedFeature& feature : kFeatures)
        if (feature.issue == issue) return &feature;
    return nullptr;
}

DeprecationDecision evaluateDeprecation(
    const DeprecatedFeature& feature,
    DeprecationRelease compilerRelease,
    bool suppressed,
    bool warningsAsErrors) noexcept
{
    if (releaseAtLeast(compilerRelease, feature.removalNotBefore))
        return {&feature, DeprecationState::Removed};
    if (suppressed) return {&feature, DeprecationState::Suppressed};
    return {&feature, warningsAsErrors ? DeprecationState::Error : DeprecationState::Warning};
}

bool deprecationRemovalAllowed(
    const DeprecatedFeature& feature,
    DeprecationRelease release,
    bool previouslyDeprecated,
    bool documentedSecurityOrUndefinedBehaviorFix) noexcept
{
    if (documentedSecurityOrUndefinedBehaviorFix) return true;
    return previouslyDeprecated && releaseAtLeast(release, feature.removalNotBefore);
}

std::optional<DeprecationRelease> parseDeprecationRelease(std::string_view text) noexcept
{
    const std::size_t separator = text.find('.');
    if (separator == std::string_view::npos || text.find('.', separator + 1) != std::string_view::npos)
        return std::nullopt;
    DeprecationRelease release;
    const auto major = std::from_chars(text.data(), text.data() + separator, release.major);
    const auto minor = std::from_chars(text.data() + separator + 1,
        text.data() + text.size(), release.minor);
    if (major.ec != std::errc{} || major.ptr != text.data() + separator ||
        minor.ec != std::errc{} || minor.ptr != text.data() + text.size())
        return std::nullopt;
    return release;
}

std::string_view deprecationStateName(DeprecationState state) noexcept
{
    switch (state)
    {
        case DeprecationState::Suppressed: return "suppressed";
        case DeprecationState::Warning: return "warning";
        case DeprecationState::Error: return "error";
        case DeprecationState::Removed: return "removed";
    }
    return "unknown";
}

std::string currentDeprecationPolicy()
{
    std::ostringstream output;
    output << "NSDEPRECATION 1\n";
    for (const DeprecatedFeature& feature : kFeatures)
    {
        output << "feature=" << feature.identity
               << " kind=" << (feature.kind == DeprecationKind::SyntaxAlias ? "syntax-alias" : "behavior")
               << " diagnostic=" << language::migrationDiagnosticCode(feature.issue)
               << " deprecated-since=" << feature.deprecatedSince.major << '.'
               << feature.deprecatedSince.minor
               << " removal-not-before=" << feature.removalNotBefore.major << '.'
               << feature.removalNotBefore.minor << '\n';
    }
    output << "exception=documented-security-or-undefined-behavior-fix\n";
    return output.str();
}
}
