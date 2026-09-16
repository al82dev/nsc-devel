#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kMigrationGuideFormatVersion = 1;

std::string currentMigrationGuide();
bool writeMigrationGuide(const std::filesystem::path& destination, std::string& error);
bool verifyMigrationGuide(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
