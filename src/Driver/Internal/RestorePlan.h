#pragma once

#include "RestoredPackage.h"

#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::driver
{
struct RestorePlan final
{
    std::vector<RestoredPackage> packages;
    std::string lock;
    std::filesystem::path root;
};
} // namespace nativesharp::driver
