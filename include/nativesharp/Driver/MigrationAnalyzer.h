#pragma once

#include "nativesharp/Language/CompatibilitySemantics.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
struct MigrationTextEdit final
{
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
    std::string replacement;
    std::string equivalenceProof;
};

struct MigrationFinding final
{
    language::MigrationIssue issue = language::MigrationIssue::NewlyContextualWord;
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::string message;
    std::string alternative;
    std::optional<MigrationTextEdit> safeFix;
};

struct MigrationAnalysis final
{
    std::filesystem::path sourcePath;
    std::vector<MigrationFinding> findings;
    bool generatedSource = false;
};

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
