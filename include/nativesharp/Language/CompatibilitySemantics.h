#pragma once

#include <span>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct BinaryModuleContract final
{
    int metadataMajor = 2;
    std::string target;
    std::string abiFingerprint;
    bool structurallyValid = true;
};
enum class ModuleCompatibility { Direct, ExplicitAdapterRequired, Rejected };
ModuleCompatibility classifyModuleCompatibility(
    const BinaryModuleContract& producer,
    const BinaryModuleContract& consumer) noexcept;

struct CompilationUnitContract final
{
    std::string canonicalModule;
    std::string sourceIdentity;
    std::vector<std::string> imports;
    std::string sourceFingerprint;
};
bool validateCompilationPlan(
    std::span<const CompilationUnitContract> units,
    std::string& error);
std::set<std::string> incrementalRebuildSet(
    std::span<const CompilationUnitContract> previous,
    std::span<const CompilationUnitContract> current);

enum class MigrationIssue
{
    NewlyContextualWord,
    OverloadResolution,
    CopyToMove,
    Nullability,
    ImportLookup,
    NumericNarrowing,
    CleanupOrder
};
std::string_view migrationDiagnosticCode(MigrationIssue issue) noexcept;

struct MigrationFix final
{
    MigrationIssue issue;
    std::string replacement;
    bool meaningProvenEquivalent = false;
};
bool mayApplyAutomaticFix(const MigrationFix& fix) noexcept;

enum class SourceCompatibility { Compatible, Breaking };
enum class AbiCompatibility { Neutral, Breaking };
struct CompatibilityClassification final
{
    SourceCompatibility source;
    AbiCompatibility abi;
    bool requiresConsumerRebuild;
};
CompatibilityClassification classifyApiChange(
    bool signatureChanged,
    bool layoutChanged,
    bool effectStrengthened,
    bool privateOnly) noexcept;

struct MigrationChapter final
{
    std::string identity;
    int order = 0;
    bool hasBeforeExample = false;
    bool hasAfterExample = false;
    bool identifiesManualDecisions = false;
};
bool validateMigrationGuide(std::span<const MigrationChapter> chapters, std::string& error);

bool removalAllowedInThreeX(
    bool securityOrUndefinedBehaviorFix,
    bool previouslyDeprecated) noexcept;
}
