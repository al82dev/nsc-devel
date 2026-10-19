#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataDispatchInterface final
{
    std::string interfaceTypeKey;
    std::uint64_t stableInterfaceTypeId = 0;
    std::string methodTableName;
    std::vector<std::string> interfaceSignatures;
    std::vector<std::string> implementationSignatures;
    std::vector<std::string> calleeNames;
};
}
