#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace nsc_internal
{
struct CompilerRunConfiguration final
{
    std::vector<std::string_view> arguments;
    std::filesystem::path logPath;
    std::filesystem::path errorLogPath;
    std::filesystem::path timeTracePath;
    std::filesystem::path failureBundlePath;
    std::string logFormat = "text";
    bool quiet = false;
    bool verbose = false;
    bool banner = true;
    bool automaticErrorLog = true;
};
}
