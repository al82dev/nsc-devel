#include "nativesharp/Language/CompatibilitySemantics.h"

#include <algorithm>
#include <map>
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

bool validateCompilationPlan(
    std::span<const CompilationUnitContract> units,
    std::string& error)
{
    error.clear();
    std::unordered_set<std::string> modules;
    std::unordered_set<std::string> identities;
    for (const CompilationUnitContract& unit : units)
    {
        if (unit.canonicalModule.empty() || unit.sourceIdentity.empty() ||
            unit.sourceFingerprint.empty() || !identities.insert(unit.sourceIdentity).second)
        {
            error = "compilation plan requires unique source identities and fingerprints";
            return false;
        }
        if (!modules.insert(unit.canonicalModule).second)
        {
            error = "a compilation plan cannot define the same module more than once";
            return false;
        }
    }
    for (const CompilationUnitContract& unit : units)
    {
        for (const std::string& imported : unit.imports)
        {
            if (!modules.contains(imported))
            {
                error = "compilation plan contains an unresolved module import";
                return false;
            }
        }
    }
    return !units.empty();
}

std::set<std::string> incrementalRebuildSet(
    std::span<const CompilationUnitContract> previous,
    std::span<const CompilationUnitContract> current)
{
    std::map<std::string, CompilationUnitContract> oldByModule;
    for (const CompilationUnitContract& unit : previous)
        oldByModule[unit.canonicalModule] = unit;
    std::set<std::string> rebuild;
    for (const CompilationUnitContract& unit : current)
    {
        const auto old = oldByModule.find(unit.canonicalModule);
        if (old == oldByModule.end() || old->second.sourceFingerprint != unit.sourceFingerprint)
            rebuild.insert(unit.canonicalModule);
    }
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const CompilationUnitContract& unit : current)
        {
            const bool importsRebuiltModule = std::ranges::any_of(
                unit.imports,
                [&](const std::string& imported) { return rebuild.contains(imported); });
            if (!rebuild.contains(unit.canonicalModule) && importsRebuiltModule)
            {
                rebuild.insert(unit.canonicalModule);
                changed = true;
            }
        }
    }
    return rebuild;
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
        const bool validChapter = !chapter.identity.empty() && chapter.order > previous &&
            chapter.hasBeforeExample && chapter.hasAfterExample &&
            chapter.identifiesManualDecisions && identities.insert(chapter.identity).second;
        if (!validChapter)
        {
            error = "migration chapters require ordered identities, before/after, and manual decisions";
            return false;
        }
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
