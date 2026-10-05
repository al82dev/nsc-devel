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
    int languageEdition = 2;
    int metadataMajor = 2;
    std::string target;
    std::string abiFingerprint;
    bool structurallyValid = true;
};
enum class ModuleCompatibility { Direct, ExplicitAdapterRequired, Rejected };
ModuleCompatibility classifyModuleCompatibility(
    const BinaryModuleContract& producer,
    const BinaryModuleContract& consumer) noexcept;

enum class EditionSemantics { NativeSharp2, NativeSharp3 };
struct EditionCompilationUnit final
{
    std::string canonicalModule;
    std::string sourceIdentity;
    int languageEdition = 3;
    EditionSemantics semantics = EditionSemantics::NativeSharp3;
    std::vector<std::string> imports;
    std::string sourceFingerprint;
};
bool validateEditionCompatibilityPlan(
    std::span<const EditionCompilationUnit> units,
    std::string& error);
std::set<std::string> editionIncrementalRebuildSet(
    std::span<const EditionCompilationUnit> previous,
    std::span<const EditionCompilationUnit> current);

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
    bool hasEdition2Before = false;
    bool hasEdition3After = false;
    bool identifiesManualDecisions = false;
};
bool validateMigrationGuide(std::span<const MigrationChapter> chapters, std::string& error);

bool removalAllowedInThreeX(
    bool securityOrUndefinedBehaviorFix,
    bool previouslyDeprecated) noexcept;
}
