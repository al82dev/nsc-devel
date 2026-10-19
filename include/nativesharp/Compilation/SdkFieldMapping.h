#pragma once

#include <string>

namespace nativesharp::compilation
{
struct SdkFieldMapping final
{
    std::string containingType;
    std::string nativeName;
    std::string nativeSharpName;
};
}
