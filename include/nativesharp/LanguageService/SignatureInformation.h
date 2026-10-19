#pragma once

#include <string>
#include <vector>

namespace nativesharp::language_service
{
struct SignatureInformation final
{
    std::string label;
    std::vector<std::string> parameters;
};
}
