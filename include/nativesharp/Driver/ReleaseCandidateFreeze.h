#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kReleaseCandidateFreezeFormatVersion = 1;

std::string currentReleaseCandidateFreeze();
bool writeReleaseCandidateFreeze(const std::filesystem::path& destination, std::string& error);
bool verifyReleaseCandidateFreeze(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
