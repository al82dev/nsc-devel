#pragma once

#include "nativesharp/Driver/MigrationAnalysis.h"
#include "nativesharp/Driver/MigrationFinding.h"
#include "nativesharp/Driver/MigrationTextEdit.h"
#include "nativesharp/Language/CompatibilitySemantics.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
MigrationAnalysis analyzeMigrationSource(
    std::filesystem::path sourcePath,
    std::string_view source);

bool analyzeMigrationFile(
    const std::filesystem::path& sourcePath,
    MigrationAnalysis& analysis,
    std::string& error);

std::string renderMigrationAnalysis(const MigrationAnalysis& analysis);

bool applySafeMigrationFixes(
    std::string_view source,
    const MigrationAnalysis& analysis,
    std::string& migratedSource,
    std::string& error);

bool applySafeMigrationFixesToFile(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    MigrationAnalysis& analysis,
    std::size_t& appliedFixCount,
    std::string& error);
}
