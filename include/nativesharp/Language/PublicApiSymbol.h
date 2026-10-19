#pragma once

#include <string>

namespace nativesharp::language
{
struct PublicApiSymbol final
{
    std::string identity;
    std::string signature;
    std::string effects;
    std::string layoutContract;
    bool isPublic = true;
};
}
