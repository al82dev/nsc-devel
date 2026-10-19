#pragma once

#include <string>
#include <vector>

namespace nativesharp::frontend::internal
{
struct MirFunctionKey final
{
    std::string mangledName;
    std::string genericSpecializationKey;
    std::string returnTypeKey;
    std::string errorTypeKey;
    std::vector<std::string> parameterTypeKeys;
};
}
