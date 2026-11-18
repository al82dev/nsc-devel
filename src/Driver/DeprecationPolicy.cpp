#include "nativesharp/Driver/DeprecationPolicy.h"

#include <array>
#include <sstream>

namespace nativesharp::driver
{
namespace
{
constexpr std::array<DeprecatedFeature, 7> kFeatures = {{
    {"contextual-identifier-alias", DeprecationKind::SyntaxAlias,
        language::MigrationIssue::NewlyContextualWord, false},
    {"legacy-null-overload", DeprecationKind::Behavior,
        language::MigrationIssue::OverloadResolution, false},
    {"implicit-owned-copy", DeprecationKind::Behavior,
        language::MigrationIssue::CopyToMove, false},
    {"implicit-null-pointer", DeprecationKind::Behavior,
        language::MigrationIssue::Nullability, false},
    {"using-import-alias", DeprecationKind::SyntaxAlias,
        language::MigrationIssue::ImportLookup, false},
    {"implicit-numeric-narrowing", DeprecationKind::Behavior,
        language::MigrationIssue::NumericNarrowing, false},
    {"legacy-cleanup-order", DeprecationKind::Behavior,
        language::MigrationIssue::CleanupOrder, false},
}};

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
    bool suppressed,
    bool warningsAsErrors) noexcept
{
    if (feature.removed)
        return {&feature, DeprecationState::Removed};
    if (suppressed) return {&feature, DeprecationState::Suppressed};
    return {&feature, warningsAsErrors ? DeprecationState::Error : DeprecationState::Warning};
}

bool deprecationRemovalAllowed(
    const DeprecatedFeature& feature,
    bool previouslyDeprecated,
    bool documentedSecurityOrUndefinedBehaviorFix) noexcept
{
    (void)feature;
    return previouslyDeprecated || documentedSecurityOrUndefinedBehaviorFix;
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
               << " status=" << (feature.removed ? "removed" : "deprecated") << '\n';
    }
    output << "exception=documented-security-or-undefined-behavior-fix\n";
    return output.str();
}
}
