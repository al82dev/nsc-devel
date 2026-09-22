#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::linker
{
inline constexpr std::uint16_t kWindowsRuntimeLibraryFormatVersion = 1;
inline constexpr std::string_view kWindowsRuntimeLibraryFileName =
    "NativeSharp.Runtime.Win32.lib";

std::filesystem::path configuredWindowsRuntimeLibrary();

std::filesystem::path resolveWindowsRuntimeLibrary(
    const std::filesystem::path& executable,
    const std::filesystem::path& configuredDefault);

bool writeWindowsRuntimeLibrary(
    const std::filesystem::path& destination,
    std::string& error);

bool verifyWindowsRuntimeLibrary(
    const std::filesystem::path& source,
    std::string& error);
}
