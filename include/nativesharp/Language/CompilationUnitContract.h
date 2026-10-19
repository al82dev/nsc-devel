#pragma once

#include <string>
#include <vector>

namespace nativesharp::language
{
struct CompilationUnitContract final
{
    std::string canonicalModule;
    std::string sourceIdentity;
    std::vector<std::string> imports;
    std::string sourceFingerprint;
};
}
