#pragma once

#include <string>
#include <vector>

namespace nativesharp::frontend::internal
{
struct ImplementationFingerprint final
{
    std::string normalizedBody;
    std::vector<std::string> usedInterfaces;
};
}
