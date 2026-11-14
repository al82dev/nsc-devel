#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace nativesharp::tests
{
// NSC-META-001: an NSMETA format label selects nothing. Given a model that a test already wrote
// under some label at `labelledPath`, this writes the same model under the current label beside it
// and reports whether both files are byte-identical. A label alone must not change what is written,
// and therefore cannot change what any reader decodes from it.
inline bool metadataLabelChangesNothing(
    interop::LibraryMetadataCodec& codec,
    const interop::LibraryMetadata& model,
    const std::filesystem::path& labelledPath,
    std::string& error)
{
    interop::LibraryMetadata unlabelled = model;
    unlabelled.formatMinor = interop::kMetadataFormatMinor;
    std::filesystem::path controlPath = labelledPath;
    controlPath.replace_extension(".current.nsmeta");
    error.clear();
    if (!codec.writeModel(unlabelled, controlPath, error)) return false;
    std::ifstream labelledStream(labelledPath, std::ios::binary);
    std::ifstream controlStream(controlPath, std::ios::binary);
    const std::string labelledBytes(
        (std::istreambuf_iterator<char>(labelledStream)), std::istreambuf_iterator<char>());
    const std::string controlBytes(
        (std::istreambuf_iterator<char>(controlStream)), std::istreambuf_iterator<char>());
    return !labelledBytes.empty() && labelledBytes == controlBytes;
}

// NSC-META-001: writes `model` under `label`, reads it back, and requires that nothing was refused or
// erased: the decode succeeds, the file equals the unlabelled write byte for byte, and when `model`
// came from a real file its ABI fingerprint -- which covers the published contracts -- is unchanged.
// This replaces the earlier down-level checks that expected the writer to refuse or strip contracts
// below a given number.
inline bool metadataSurvivesLabel(
    interop::LibraryMetadataCodec& codec,
    const interop::LibraryMetadata& model,
    std::uint16_t label,
    const std::filesystem::path& path,
    std::string& error)
{
    interop::LibraryMetadata labelled = model;
    labelled.formatMinor = label;
    error.clear();
    if (!codec.writeModel(labelled, path, error)) return false;
    interop::LibraryMetadata decoded;
    if (!codec.read(path, decoded, error)) return false;
    if (!metadataLabelChangesNothing(codec, labelled, path, error))
    {
        if (error.empty()) error = "the label changed the written bytes";
        return false;
    }
    const std::array<std::uint8_t, 32> unset{};
    if (model.abiFingerprint != unset && decoded.abiFingerprint != model.abiFingerprint)
    {
        error = "the decoded ABI fingerprint differs from the source model";
        return false;
    }
    return true;
}
}
