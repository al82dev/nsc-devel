#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"

#include "../Frontend/Cache/CachePayloadReader.h"
#include "../Frontend/Cache/CachePayloadWriter.h"
#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"

#include <cstdint>
#include <fstream>
#include <limits>
#include <utility>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view SidecarSchema = "nsc-precompiled-product-sidecar-v1";
// EN: v2 appends the semantic snapshot of the public API unit after the four v1 products. A
//     sidecar without a snapshot is still written as v1, byte for byte as before, and both are read.
// ES: v2 agrega el snapshot semántico de la unidad de API pública después de los cuatro productos
//     de v1. Un sidecar sin snapshot se sigue escribiendo como v1, byte por byte como antes, y se
//     leen ambos.
constexpr std::string_view SidecarSchemaV2 = "nsc-precompiled-product-sidecar-v2";

std::string metadataDigest(std::span<const std::uint8_t> metadataBytes)
{
    return basic::sha256Hex(basic::sha256(metadataBytes));
}

std::string payloadDigest(std::string_view payload)
{
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

void encodeBody(
    std::string& bytes,
    std::string_view schema,
    std::string_view buildId,
    std::uint64_t metadataSize,
    std::string_view digest,
    const PrecompiledProductSidecar& sidecar)
{
    frontend::internal::CachePayloadWriter::string(bytes, schema);
    frontend::internal::CachePayloadWriter::string(bytes, buildId);
    frontend::internal::CachePayloadWriter::u64(bytes, metadataSize);
    frontend::internal::CachePayloadWriter::string(bytes, digest);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicApiSource);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicTemplateSource);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicApiSyntax);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicTemplateSyntax);
    if (schema == SidecarSchemaV2)
        frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicApiSnapshot);
}
}

std::filesystem::path PrecompiledProductSidecarCodec::pathForMetadata(
    const std::filesystem::path& metadataPath)
{
    return precompiledProductSidecarPath(metadataPath);
}

bool PrecompiledProductSidecarCodec::write(
    const std::filesystem::path& metadataPath,
    const PrecompiledProductSidecar& sidecar,
    std::span<const std::uint8_t> metadataBytes,
    std::string& error)
{
    const std::string encoded = encode(sidecar, metadataBytes);
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size()};
    if (!basic::writeFileTransactionally(pathForMetadata(metadataPath), bytes, error))
    {
        error = "cannot write precompiled product sidecar: " + error;
        return false;
    }
    return true;
}

bool PrecompiledProductSidecarCodec::read(
    const std::filesystem::path& sidecarPath,
    std::span<const std::uint8_t> metadataBytes,
    PrecompiledProductSidecar& sidecar,
    std::string& error)
{
    std::ifstream stream(sidecarPath, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        error = "cannot open precompiled product sidecar '" + sidecarPath.string() + "'";
        return false;
    }
    const std::streampos end = stream.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        static_cast<std::uintmax_t>(end) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
    {
        error = "precompiled product sidecar has invalid size";
        return false;
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    stream.seekg(0, std::ios::beg);
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty())
    {
        error = "failed while reading precompiled product sidecar";
        return false;
    }
    return decode(bytes, metadataBytes, sidecar, error);
}

std::string PrecompiledProductSidecarCodec::encode(
    const PrecompiledProductSidecar& sidecar,
    std::span<const std::uint8_t> metadataBytes)
{
    std::string bytes;
    encodeBody(
        bytes,
        sidecar.publicApiSnapshot.empty() ? SidecarSchema : SidecarSchemaV2,
        kCompilerBuildId,
        static_cast<std::uint64_t>(metadataBytes.size()),
        metadataDigest(metadataBytes),
        sidecar);
    const std::string checksum = payloadDigest(bytes);
    frontend::internal::CachePayloadWriter::string(bytes, checksum);
    return bytes;
}

bool PrecompiledProductSidecarCodec::decode(
    std::string_view bytes,
    std::span<const std::uint8_t> metadataBytes,
    PrecompiledProductSidecar& sidecar,
    std::string& error)
{
    frontend::internal::CachePayloadReader reader(bytes);
    std::string schema;
    std::string buildId;
    std::uint64_t metadataSize = 0;
    std::string digest;
    PrecompiledProductSidecar decoded;
    std::string checksum;
    if (!reader.string(schema) || !reader.string(buildId) ||
        !reader.u64(metadataSize) || !reader.string(digest) ||
        !reader.string(decoded.publicApiSource) ||
        !reader.string(decoded.publicTemplateSource) ||
        !reader.string(decoded.publicApiSyntax) ||
        !reader.string(decoded.publicTemplateSyntax) ||
        (schema == SidecarSchemaV2 && !reader.string(decoded.publicApiSnapshot)) ||
        !reader.string(checksum) || !reader.remaining().empty())
    {
        error = "precompiled product sidecar is truncated or malformed";
        return false;
    }
    if (schema != SidecarSchema && schema != SidecarSchemaV2)
    {
        error = "precompiled product sidecar schema mismatch";
        return false;
    }

    std::string body;
    encodeBody(body, schema, buildId, metadataSize, digest, decoded);
    if (payloadDigest(body) != checksum)
    {
        error = "precompiled product sidecar checksum mismatch";
        return false;
    }
    if (buildId != kCompilerBuildId)
    {
        error = "precompiled product sidecar compiler build-id mismatch";
        return false;
    }
    if (metadataSize != static_cast<std::uint64_t>(metadataBytes.size()))
    {
        error = "precompiled product sidecar NSMETA size mismatch";
        return false;
    }
    if (digest != metadataDigest(metadataBytes))
    {
        error = "precompiled product sidecar NSMETA digest mismatch";
        return false;
    }

    sidecar = std::move(decoded);
    error.clear();
    return true;
}
}
