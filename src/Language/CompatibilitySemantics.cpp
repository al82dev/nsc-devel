#include "nativesharp/Language/CompatibilitySemantics.h"

#include <algorithm>
#include <unordered_set>

namespace nativesharp::language
{
ModuleCompatibility classifyModuleCompatibility(
    const BinaryModuleContract& producer,
    const BinaryModuleContract& consumer) noexcept
{
    if (!producer.structurallyValid || !consumer.structurallyValid ||
        producer.target != "windows-x64" || consumer.target != "windows-x64" ||
        producer.metadataMajor > consumer.metadataMajor)
        return ModuleCompatibility::Rejected;
    if (producer.abiFingerprint == consumer.abiFingerprint)
        return ModuleCompatibility::Direct;
    return ModuleCompatibility::ExplicitAdapterRequired;
}

std::string_view migrationDiagnosticCode(MigrationIssue issue) noexcept
{
    switch (issue)
    {
        case MigrationIssue::NewlyContextualWord: return "MIG3001";
        case MigrationIssue::OverloadResolution: return "MIG3002";
        case MigrationIssue::CopyToMove: return "MIG3003";
        case MigrationIssue::Nullability: return "MIG3004";
        case MigrationIssue::ImportLookup: return "MIG3005";
        case MigrationIssue::NumericNarrowing: return "MIG3006";
        case MigrationIssue::CleanupOrder: return "MIG3007";
    }
    return "MIG3000";
}

bool mayApplyAutomaticFix(const MigrationFix& fix) noexcept
{
    return fix.meaningProvenEquivalent && !fix.replacement.empty();
}

CompatibilityClassification classifyApiChange(
    bool signatureChanged,
    bool layoutChanged,
    bool effectStrengthened,
    bool privateOnly) noexcept
{
    if (privateOnly) return {SourceCompatibility::Compatible, AbiCompatibility::Neutral, false};
    const bool sourceBreak = signatureChanged || effectStrengthened;
    const bool abiBreak = signatureChanged || layoutChanged;
    return {sourceBreak ? SourceCompatibility::Breaking : SourceCompatibility::Compatible,
        abiBreak ? AbiCompatibility::Breaking : AbiCompatibility::Neutral,
        sourceBreak || abiBreak};
}

bool validateMigrationGuide(
    std::span<const MigrationChapter> chapters,
    std::string& error)
{
    error.clear();
    if (chapters.empty()) { error = "migration guide has no executable chapters"; return false; }
    std::unordered_set<std::string> identities;
    int previous = -1;
    for (const auto& chapter : chapters)
    {
        if (chapter.identity.empty() || chapter.order <= previous || !chapter.hasEdition2Before ||
            !chapter.hasEdition3After || !chapter.identifiesManualDecisions ||
            !identities.insert(chapter.identity).second)
        { error = "migration chapters require ordered identities, before/after, and manual decisions"; return false; }
        previous = chapter.order;
    }
    return true;
}

bool removalAllowedInThreeX(
    bool securityOrUndefinedBehaviorFix,
    bool previouslyDeprecated) noexcept
{
    return securityOrUndefinedBehaviorFix || previouslyDeprecated;
}
}
