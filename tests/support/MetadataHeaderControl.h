#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace nativesharp::tests
{
// Compare deterministic writes and verify the same content with different reserved header bytes.
inline bool metadataReservedBytesPreserveContent(
    interop::LibraryMetadataCodec& codec,
    const interop::LibraryMetadata& model,
    const std::filesystem::path& sourcePath,
    std::string& error)
{
    std::filesystem::path controlPath = sourcePath;
    controlPath.replace_extension(".current.nsmeta");
    error.clear();
    if (!codec.writeModel(model, controlPath, error)) return false;
    std::ifstream sourceStream(sourcePath, std::ios::binary);
    std::ifstream controlStream(controlPath, std::ios::binary);
    const std::string sourceBytes(
        (std::istreambuf_iterator<char>(sourceStream)), std::istreambuf_iterator<char>());
    const std::string controlBytes(
        (std::istreambuf_iterator<char>(controlStream)), std::istreambuf_iterator<char>());
    if (sourceBytes.size() < 12U || sourceBytes != controlBytes) return false;
    std::vector<std::uint8_t> varied(sourceBytes.begin(), sourceBytes.end());
    varied[8] = 0x5aU;
    varied[9] = 0xa5U;
    varied[10] = 0xc3U;
    varied[11] = 0x3cU;
    interop::LibraryMetadata original;
    interop::LibraryMetadata decoded;
    return codec.read(sourcePath, original, error) && codec.read(varied, decoded, error) &&
        original.abiFingerprint == decoded.abiFingerprint;
}

// Write a model, vary its reserved bytes and verify its decoded ABI fingerprint.
inline bool metadataSurvivesReservedBytes(
    interop::LibraryMetadataCodec& codec,
    const interop::LibraryMetadata& model,
    std::uint16_t reservedValue,
    const std::filesystem::path& path,
    std::string& error)
{
    error.clear();
    if (!codec.writeModel(model, path, error)) return false;
    if (!metadataReservedBytesPreserveContent(codec, model, path, error))
    {
        if (error.empty()) error = "metadata content or reserved-byte control differed";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    if (bytes.size() < 12U) { error = "metadata header is truncated"; return false; }
    bytes[10] = static_cast<char>(reservedValue & 0xffU);
    bytes[11] = static_cast<char>(reservedValue >> 8U);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) { error = "cannot write metadata header bytes"; return false; }
    interop::LibraryMetadata decoded;
    if (!codec.read(path, decoded, error)) return false;
    const std::array<std::uint8_t, 32> unset{};
    if (model.abiFingerprint != unset && decoded.abiFingerprint != model.abiFingerprint)
    {
        error = "the decoded ABI fingerprint differs from the source model";
        return false;
    }
    return true;
}
}
