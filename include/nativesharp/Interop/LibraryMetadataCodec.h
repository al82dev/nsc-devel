#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

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

    bool verifyCurrentCompiler(
        const LibraryMetadata& metadata,
        std::string& error) const;

    static std::string fingerprintHex(const std::array<std::uint8_t, 32>& fingerprint);

private:
    bool writeModelImpl(
        const LibraryMetadata& model,
        const std::filesystem::path& destination,
        LibraryMetadata* writtenModel,
        std::string& error) const;
};
}
