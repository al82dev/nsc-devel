#pragma once

#include <string>
#include <vector>

namespace nativesharp::frontend::internal
{
struct InterfaceFingerprint final
{
    std::string signature;
    std::string layoutAbi;
    std::vector<std::string> publicDependencies;
};
}
