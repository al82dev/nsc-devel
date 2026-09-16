#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kFinalBaselineContractFormatVersion = 1;

std::string currentFinalBaselineContract();
bool writeFinalBaselineContract(const std::filesystem::path& destination, std::string& error);
bool verifyFinalBaselineContract(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
