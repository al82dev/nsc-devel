#pragma once

#include <span>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/AbiCompatibility.h"
#include "nativesharp/Language/BinaryModuleContract.h"
#include "nativesharp/Language/CompatibilityClassification.h"
#include "nativesharp/Language/CompilationUnitContract.h"
#include "nativesharp/Language/MigrationChapter.h"
#include "nativesharp/Language/MigrationFix.h"
#include "nativesharp/Language/MigrationIssue.h"
#include "nativesharp/Language/ModuleCompatibility.h"
#include "nativesharp/Language/SourceCompatibility.h"

namespace nativesharp::language
{
ModuleCompatibility classifyModuleCompatibility(
    const BinaryModuleContract& producer,
    const BinaryModuleContract& consumer) noexcept;

bool validateCompilationPlan(
    std::span<const CompilationUnitContract> units,
    std::string& error);
std::set<std::string> incrementalRebuildSet(
    std::span<const CompilationUnitContract> previous,
    std::span<const CompilationUnitContract> current);

std::string_view migrationDiagnosticCode(MigrationIssue issue) noexcept;

bool mayApplyAutomaticFix(const MigrationFix& fix) noexcept;

CompatibilityClassification classifyApiChange(
    bool signatureChanged,
    bool layoutChanged,
    bool effectStrengthened,
    bool privateOnly) noexcept;

bool validateMigrationGuide(std::span<const MigrationChapter> chapters, std::string& error);

bool removalAllowedInThreeX(
    bool securityOrUndefinedBehaviorFix,
    bool previouslyDeprecated) noexcept;
}
