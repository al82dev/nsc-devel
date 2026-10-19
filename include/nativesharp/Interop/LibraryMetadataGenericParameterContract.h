#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataGenericParameterContract final
{
    std::string name;
    std::uint8_t kind = 0;
    std::uint16_t flags = 0;
    std::string constantTypeKey;
    std::string equalityTypeKey;
    std::string associatedProjectionKey;
    std::vector<std::string> typeConstraintKeys;
};
}
