#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kReproducibilityReportFormatVersion = 1;
inline constexpr unsigned kInstallationUpdatePlanFormatVersion = 1;

struct ReproducibilityComparison final
{
    bool reproducible = false;
    std::size_t deterministicEntries = 0;
    std::size_t volatileEntries = 0;
    std::size_t differences = 0;
    std::string report;
};

bool compareInstallationManifests(const std::filesystem::path &left,
    const std::filesystem::path &right,
    ReproducibilityComparison &comparison,
    std::string &error);

bool writeInstallationUpdatePlan(const std::filesystem::path &currentManifest,
    const std::filesystem::path &nextManifest,
    const std::filesystem::path &destination,
    std::string &error);

bool writeInstallationRemovalPlan(
    const std::filesystem::path &currentManifest, const std::filesystem::path &destination, std::string &error);

bool applyInstallationRemovalPlan(const std::filesystem::path &prefix,
    const std::filesystem::path &currentManifest,
    const std::filesystem::path &nextManifest,
    const std::filesystem::path &plan,
    std::string &error);
} // namespace nativesharp::driver
