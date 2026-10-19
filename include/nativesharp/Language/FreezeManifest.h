#pragma once

#include <string>

namespace nativesharp::language
{
struct FreezeManifest final
{
    std::string syntaxRevision;
    std::string semanticRevision;
    std::string win64AbiRevision;
    std::string metadataRevision;
};
}
