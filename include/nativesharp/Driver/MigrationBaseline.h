#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kMigrationBaselineFormatVersion = 1;

// Canonical snapshot of the 1.x contracts that a future 2.0 proposal must
// classify as preserved, migrated, or intentionally broken.
std::string currentMigrationBaseline();

bool writeMigrationBaseline(const std::filesystem::path& destination, std::string& error);

bool verifyMigrationBaseline(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
