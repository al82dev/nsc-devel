#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
struct BuildResult final
{
    std::filesystem::path project;
    std::filesystem::path artifact;
    std::string compilerOutput;
    std::string compilerError;
};
} // namespace nativesharp::driver
