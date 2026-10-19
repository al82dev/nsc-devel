#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataGenericSpecialization final
{
    std::uint8_t kind = 0;
    std::string templateKey;
    std::string specializationKey;
    std::string specializedName;
    std::vector<std::string> typeArguments;
};
}
