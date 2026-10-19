#pragma once

#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataExport final
{
    std::string cAbiName;
    std::string declaringTypeKey;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
    std::string nativeMangledName;
};
}
