#include "nativesharp/LLVM/ThinLtoLibrarySidecarCodec.h"

#include "nativesharp/Basic/Sha256.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string_view>
#include <utility>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr std::string_view kSignature = "NSTHINLT";
constexpr std::uint32_t kVersion = 2;
// A library has one unit per code partition; this only bounds a corrupt count.
// Una biblioteca tiene una unidad por particion de codigo; esto solo acota un conteo corrupto.
constexpr std::uint32_t kMaximumUnits = 65536;
constexpr std::size_t kDigestSize = 32;

void appendInteger(std::string& bytes, std::uint64_t value, std::size_t size)
{
    for (std::size_t index = 0; index < size; ++index)
        bytes.push_back(static_cast<char>((value >> (8U * index)) & 0xFFU));
}

void appendString(std::string& bytes, std::string_view text)
{
    appendInteger(bytes, text.size(), 4);
    bytes.append(text);
}

basic::Sha256Digest digestOf(std::string_view bytes)
{
    return basic::sha256(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
}

// Sequential reads over the sidecar bytes from `offset`; each fails instead of running past the end.
// Lecturas secuenciales sobre los bytes del sidecar desde `offset`; cada una falla en vez de pasar del final.
bool readInteger(std::string_view bytes, std::size_t& offset, std::uint64_t& value, std::size_t size) noexcept
{
    if (bytes.size() - offset < size) return false;
    value = 0;
    for (std::size_t index = 0; index < size; ++index)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset + index])) << (8U * index);
    offset += size;
    return true;
}

bool readText(std::string_view bytes, std::size_t& offset, std::string& value, std::size_t size)
{
    if (bytes.size() - offset < size) return false;
    value.assign(bytes.substr(offset, size));
    offset += size;
    return true;
}

bool readString(std::string_view bytes, std::size_t& offset, std::string& value)
{
    std::uint64_t size = 0;
    return readInteger(bytes, offset, size, 4) && readText(bytes, offset, value, static_cast<std::size_t>(size));
}

bool decode(std::string_view bytes, ThinLtoLibrarySidecar& sidecar, std::string& error)
{
    if (bytes.size() < kSignature.size() + kDigestSize || bytes.substr(0, kSignature.size()) != kSignature)
    {
        error = "not a NativeSharp ThinLTO sidecar";
        return false;
    }
    const std::string_view body = bytes.substr(0, bytes.size() - kDigestSize);
    const basic::Sha256Digest digest = digestOf(body);
    if (std::string_view(reinterpret_cast<const char*>(digest.data()), digest.size()) !=
        bytes.substr(body.size()))
    {
        error = "the sidecar integrity digest does not match";
        return false;
    }
    std::size_t offset = 0;
    std::string signature;
    std::uint64_t version = 0;
    std::uint64_t count = 0;
    ThinLtoLibrarySidecar decoded;
    if (!readText(body, offset, signature, kSignature.size()) || !readInteger(body, offset, version, 4) ||
        version != kVersion)
    {
        error = "unsupported ThinLTO sidecar version";
        return false;
    }
    if (!readString(body, offset, decoded.compatibility) || !readString(body, offset, decoded.libraryDigest) ||
        !readString(body, offset, decoded.metadataDigest) || !readString(body, offset, decoded.dependencies) ||
        !readInteger(body, offset, count, 4) || count == 0 || count > kMaximumUnits)
    {
        error = "truncated or invalid ThinLTO sidecar header";
        return false;
    }
    decoded.units.resize(static_cast<std::size_t>(count));
    for (std::string& unit : decoded.units)
    {
        std::uint64_t size = 0;
        if (!readInteger(body, offset, size, 8) || size == 0 || size > body.size() ||
            !readText(body, offset, unit, static_cast<std::size_t>(size)))
        {
            error = "truncated or invalid ThinLTO sidecar unit";
            return false;
        }
    }
    if (offset != body.size())
    {
        error = "the ThinLTO sidecar has trailing bytes";
        return false;
    }
    sidecar = std::move(decoded);
    return true;
}
}

std::filesystem::path ThinLtoLibrarySidecarCodec::pathFor(const std::filesystem::path& library)
{
    std::filesystem::path sidecar = library;
    sidecar.replace_extension(".nsthinlto");
    return sidecar;
}

bool ThinLtoLibrarySidecarCodec::write(
    const std::filesystem::path& path,
    const ThinLtoLibrarySidecar& sidecar,
    std::string& error)
{
    std::string bytes(kSignature);
    appendInteger(bytes, kVersion, 4);
    appendString(bytes, sidecar.compatibility);
    appendString(bytes, sidecar.libraryDigest);
    appendString(bytes, sidecar.metadataDigest);
    appendString(bytes, sidecar.dependencies);
    appendInteger(bytes, sidecar.units.size(), 4);
    for (const std::string& unit : sidecar.units)
    {
        appendInteger(bytes, unit.size(), 8);
        bytes.append(unit);
    }
    const basic::Sha256Digest digest = digestOf(bytes);
    bytes.append(reinterpret_cast<const char*>(digest.data()), digest.size());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
    {
        error = "cannot write the ThinLTO sidecar '" + path.string() + "'";
        return false;
    }
    return true;
}

bool ThinLtoLibrarySidecarCodec::read(
    const std::filesystem::path& path,
    ThinLtoLibrarySidecar& sidecar,
    std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "cannot open the ThinLTO sidecar '" + path.string() + "'";
        return false;
    }
    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "cannot read the ThinLTO sidecar '" + path.string() + "'";
        return false;
    }
    return decode(bytes, sidecar, error);
}
}
