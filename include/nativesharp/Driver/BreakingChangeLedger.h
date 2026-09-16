#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kBreakingChangeLedgerFormatVersion = 1;

// Canonical, ordered inventory of intentional incompatibilities between the
// final 1.0.1 contract and NativeSharp 2.0.
std::string currentBreakingChangeLedger();

bool writeBreakingChangeLedger(const std::filesystem::path& destination, std::string& error);

bool verifyBreakingChangeLedger(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
