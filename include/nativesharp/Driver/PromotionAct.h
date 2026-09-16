#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
inline constexpr unsigned kPromotionActFormatVersion = 1;

std::string currentPromotionAct();
bool writePromotionAct(const std::filesystem::path& destination, std::string& error);
bool verifyPromotionAct(const std::filesystem::path& source, std::string& error);
} // namespace nativesharp::driver
