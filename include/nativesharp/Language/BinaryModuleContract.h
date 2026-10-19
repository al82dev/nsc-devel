#pragma once

#include <string>

namespace nativesharp::language
{
struct BinaryModuleContract final
{
    int metadataMajor = 2;
    std::string target;
    std::string abiFingerprint;
    bool structurallyValid = true;
};
}
