#pragma once

#include <string>
#include <vector>

namespace nativesharp::driver
{
namespace
{
struct PrecompiledExportContract final
{
    std::string moduleName;
    std::string runtimeName;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
};
} // namespace
} // namespace nativesharp::driver
