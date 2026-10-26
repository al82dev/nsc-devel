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
    // EN: Freeze 3.0 D-10: the sequenced error.log in the working directory is opt-in (--error-log).
    // ES: Congelamiento 3.0 D-10: el error.log secuencial en el directorio de trabajo es opcional
    //     (--error-log).
    bool automaticErrorLog = false;
};
}
