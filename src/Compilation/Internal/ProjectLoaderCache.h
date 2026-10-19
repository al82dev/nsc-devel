#pragma once

#include "ProjectDescriptor.h"

#include "nativesharp/Compilation/ProjectLoaderStatistics.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace nativesharp::compilation
{
class ProjectLoaderCache final
{
public:
    mutable std::mutex mutex;
    std::unordered_map<std::string, ProjectDescriptor> descriptors;
    ProjectLoaderStatistics statistics;
};
}
