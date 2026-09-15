#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
inline constexpr unsigned kCompatibilityPolicyFormatVersion = 1;
inline constexpr unsigned kCompatibilityMatrixFormatVersion = 1;

std::string currentCompatibilityPolicy();

bool writeCompatibilityPolicy(const std::filesystem::path &destination, std::string &error);

bool verifyCompatibilityPolicy(const std::filesystem::path &source, std::string &error);

bool checkCompilerCompatibility(std::string_view requiredVersion, std::string &explanation, std::string &error);

std::string currentCompatibilityMatrix();
bool writeCompatibilityMatrix(const std::filesystem::path& destination, std::string& error);
bool verifyCompatibilityMatrix(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
