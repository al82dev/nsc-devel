#pragma once

#include "nativesharp/Driver/MigrationAnalyzer.h"

#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::driver
{
struct ExecutableMigrationStep final
{
    MigrationFinding finding;
    std::string before;
    std::string after;
    bool automatic = false;
};

struct ExecutableMigrationChapter final
{
    std::string identity;
    int order = 0;
    std::vector<ExecutableMigrationStep> steps;
};

struct ExecutableMigrationGuide final
{
    std::filesystem::path sourcePath;
    std::vector<ExecutableMigrationChapter> chapters;
};

ExecutableMigrationGuide buildExecutableMigrationGuide(
    const MigrationAnalysis& analysis,
    std::string_view source);

bool validateExecutableMigrationGuide(
    const ExecutableMigrationGuide& guide,
    std::string& error);

std::string renderExecutableMigrationGuide(
    const ExecutableMigrationGuide& guide);

bool writeExecutableMigrationGuide(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    ExecutableMigrationGuide& guide,
    std::string& error);
}
