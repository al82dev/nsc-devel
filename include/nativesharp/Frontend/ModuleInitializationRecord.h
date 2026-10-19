#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ModuleInitializationRecord final
{
    std::size_t projectOrdinal = 0;
    std::string moduleName;
    std::vector<std::size_t> dependencyProjectOrdinals;
    std::vector<std::size_t> staticConstructorMemberOrdinals;
};
}
