#pragma once

#include "nativesharp/Compilation/PrecompiledProductSidecar.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::compilation
{
class PrecompiledProductSidecarCodec final
{
public:
    static std::string encode(
        const PrecompiledProductSidecar& sidecar,
        std::span<const std::uint8_t> metadataBytes);

    static bool decode(
        std::string_view bytes,
        std::span<const std::uint8_t> metadataBytes,
        PrecompiledProductSidecar& sidecar,
        std::string& error);
};
}
