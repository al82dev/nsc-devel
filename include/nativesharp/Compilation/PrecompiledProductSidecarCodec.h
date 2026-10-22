#pragma once

#include "nativesharp/Compilation/PrecompiledProductSidecar.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::compilation
{
class PrecompiledProductSidecarCodec final
{
public:
    static std::filesystem::path pathForMetadata(
        const std::filesystem::path& metadataPath);

    static bool write(
        const std::filesystem::path& metadataPath,
        const PrecompiledProductSidecar& sidecar,
        std::span<const std::uint8_t> metadataBytes,
        std::string& error);

    // Reads one sidecar and validates its checksum, compiler build-id and exact NSMETA identity.
    static bool read(
        const std::filesystem::path& sidecarPath,
        std::span<const std::uint8_t> metadataBytes,
        PrecompiledProductSidecar& sidecar,
        std::string& error);

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
