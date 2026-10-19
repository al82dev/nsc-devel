#pragma once

#include <string>

namespace nativesharp::language
{
struct ImplCandidate final
{
    std::string traitIdentity;
    std::string typeIdentity;
    std::string moduleIdentity;
    bool traitOwnedByModule = false;
    bool typeOwnedByModule = false;
};
}
