#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"

#include "../Frontend/Cache/CachePayloadReader.h"
#include "../Frontend/Cache/CachePayloadWriter.h"
#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"

#include <cstdint>
#include <utility>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view SidecarSchema = "nsc-precompiled-product-sidecar-v1";

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
    std::string_view buildId,
    std::uint64_t metadataSize,
    std::string_view digest,
    const PrecompiledProductSidecar& sidecar)
{
    frontend::internal::CachePayloadWriter::string(bytes, SidecarSchema);
    frontend::internal::CachePayloadWriter::string(bytes, buildId);
    frontend::internal::CachePayloadWriter::u64(bytes, metadataSize);
    frontend::internal::CachePayloadWriter::string(bytes, digest);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicApiSource);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicTemplateSource);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicApiSyntax);
    frontend::internal::CachePayloadWriter::string(bytes, sidecar.publicTemplateSyntax);
}
}

std::filesystem::path PrecompiledProductSidecarCodec::pathForMetadata(
    const std::filesystem::path& metadataPath)
{
    std::filesystem::path path = metadataPath;
    path.replace_extension(".nsproducts");
    return path;
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

std::string PrecompiledProductSidecarCodec::encode(
    const PrecompiledProductSidecar& sidecar,
    std::span<const std::uint8_t> metadataBytes)
{
    std::string bytes;
    encodeBody(
        bytes,
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
        !reader.string(checksum) || !reader.remaining().empty())
    {
        error = "precompiled product sidecar is truncated or malformed";
        return false;
    }
    if (schema != SidecarSchema)
    {
        error = "precompiled product sidecar schema mismatch";
        return false;
    }

    std::string body;
    encodeBody(body, buildId, metadataSize, digest, decoded);
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
