#pragma once

#include <string>

namespace nativesharp::compilation
{
struct SdkTargetVariant final
{
    std::string architecture;
    std::string minimumVersion;
    std::string maximumVersion;
};
}
