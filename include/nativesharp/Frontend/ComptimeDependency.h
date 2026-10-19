#pragma once

#include <string>

namespace nativesharp::frontend
{
struct ComptimeDependency final
{
    std::string kind;
    std::string identity;
    std::string digest;
};
}
