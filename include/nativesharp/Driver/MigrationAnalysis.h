#pragma once

#include "nativesharp/Driver/MigrationFinding.h"

#include <filesystem>
#include <vector>

namespace nativesharp::driver
{
struct MigrationAnalysis final
{
    std::filesystem::path sourcePath;
    std::vector<MigrationFinding> findings;
    bool generatedSource = false;
};
} // namespace nativesharp::driver
