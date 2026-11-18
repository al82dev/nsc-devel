#pragma once

#include <string>

namespace nativesharp::language
{
struct BinaryModuleContract final
{
    std::string target;
    std::string abiFingerprint;
    bool structurallyValid = true;
};
}
