#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kBreakingChangeLedgerFormatVersion = 1;

// Canonical, ordered inventory of intentional changes to physical contracts.
std::string currentBreakingChangeLedger();

bool writeBreakingChangeLedger(const std::filesystem::path& destination, std::string& error);

bool verifyBreakingChangeLedger(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
