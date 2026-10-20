#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
std::string jsonEscape(std::string_view value);
std::string pathText(const std::filesystem::path &path);
std::optional<std::string> environmentValue(const char *name);
} // namespace nativesharp::driver
