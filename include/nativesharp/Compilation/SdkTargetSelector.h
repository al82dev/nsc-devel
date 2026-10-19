#pragma once

#include <string>
#include <string_view>

namespace nativesharp::compilation
{
struct SdkManifest;
struct SdkTargetVariant;

class SdkTargetSelector final
{
public:
    const SdkTargetVariant* select(
        const SdkManifest& manifest,
        std::string_view architecture,
        std::string_view version,
        std::string& error) const;
};
}
