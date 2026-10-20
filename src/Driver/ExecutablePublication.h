#pragma once

#include <filesystem>

namespace nativesharp::driver
{
struct ExecutablePublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
    std::filesystem::path pdbDestination;
    std::filesystem::path pdbStage;
    std::filesystem::path stateDestination;
    std::filesystem::path stateStage;
};
} // namespace nativesharp::driver
