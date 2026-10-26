#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::interop
{
class LibraryMetadataCodec final
{
public:
    bool write(
        const frontend::ProjectCompilation& compilation,
        const std::filesystem::path& destination,
        std::string& error) const;

    bool write(
        const frontend::ProjectCompilation& compilation,
        const std::filesystem::path& destination,
        LibraryMetadata& writtenModel,
        std::string& error) const;

    bool write(
        const frontend::ProjectCompilation& compilation,
        const std::filesystem::path& destination,
        LibraryMetadata& writtenModel,
        std::vector<std::uint8_t>& writtenBytes,
        std::string& error) const;

    // Escribe un modelo ya construido, en la version menor que el modelo dice. Es lo que permite
    // MEDIR la compatibilidad hacia atras en vez de afirmarla: produce un fichero de una version
    // anterior y leelo con el lector actual.
    bool writeModel(
        const LibraryMetadata& model,
        const std::filesystem::path& destination,
        std::string& error) const;

    bool read(
        const std::filesystem::path& source,
        LibraryMetadata& metadata,
        std::string& error) const;

    bool read(
        std::span<const std::uint8_t> bytes,
        LibraryMetadata& metadata,
        std::string& error) const;
    // Decodes bytes whose content was already authenticated by a previous read (the repository
    // records the content digest of every file that passed verification). The header is checked as
    // in `read`; the payload checksum and the ABI fingerprint are not recomputed.
    bool readAuthenticated(
        std::span<const std::uint8_t> bytes,
        LibraryMetadata& metadata,
        std::string& error) const;

    bool verifyCurrentCompiler(
        const LibraryMetadata& metadata,
        std::string& error) const;
    // EN: Rejects a library produced before the 3.0 layout epoch (kMetadataLayoutEpochMinor).
    // ES: Rechaza una biblioteca producida antes de la época de layout 3.0.
    bool verifyLayoutEpoch(
        const LibraryMetadata& metadata,
        std::string& error) const;

    static std::string fingerprintHex(const std::array<std::uint8_t, 32>& fingerprint);

private:
    bool readBytes(
        std::span<const std::uint8_t> bytes,
        bool authenticate,
        LibraryMetadata& metadata,
        std::string& error) const;
    bool writeModelImpl(
        const LibraryMetadata& model,
        const std::filesystem::path& destination,
        LibraryMetadata* writtenModel,
        std::vector<std::uint8_t>* writtenBytes,
        std::string& error) const;
};
}
