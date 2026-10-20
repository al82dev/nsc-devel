#include "nativesharp/Linker/PdbVerifier.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <vector>

namespace nativesharp::linker
{
namespace
{
constexpr char kMsfMagic[] = "Microsoft C/C++ MSF 7.00\r\n\x1a" "DS\0\0\0";
constexpr std::size_t kSuperBlockSize = 56;
constexpr std::uint32_t kAbsentStream = 0xffffffffU;
constexpr std::uint32_t kFileChecksumSubsection = 0xf4U;
constexpr std::uint64_t kMaximumPdbBytes = 1024ULL * 1024ULL * 1024ULL;

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

bool rangeFits(std::size_t offset, std::size_t size, std::size_t total)
{
    return offset <= total && size <= total - offset;
}

bool validBlockSize(std::uint32_t blockSize)
{
    return blockSize >= 512 && blockSize <= 65536 && (blockSize & (blockSize - 1U)) == 0;
}

bool containsChecksumSubsection(const std::vector<std::uint8_t>& stream)
{
    for (std::size_t offset = 0; rangeFits(offset, 8, stream.size()); offset += 4)
    {
        if (readU32(stream, offset) != kFileChecksumSubsection) continue;
        const std::uint32_t payloadSize = readU32(stream, offset + 4);
        if (payloadSize != 0 && rangeFits(offset + 8, payloadSize, stream.size())) return true;
    }
    return false;
}

bool readPdbBytes(
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes,
    std::string& error)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
    {
        error = "could not open PDB";
        return false;
    }
    const std::streamoff length = input.tellg();
    if (length < static_cast<std::streamoff>(kSuperBlockSize) ||
        static_cast<std::uint64_t>(length) > std::numeric_limits<std::size_t>::max() ||
        static_cast<std::uint64_t>(length) > kMaximumPdbBytes)
    {
        error = "PDB has an invalid or unsupported MSF file size (maximum 1 GiB)";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(length));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input)
    {
        error = "could not read the complete PDB";
        return false;
    }
    if (std::memcmp(bytes.data(), kMsfMagic, sizeof(kMsfMagic) - 1) != 0)
    {
        error = "PDB does not contain the MSF 7.00 signature";
        return false;
    }
    return true;
}

bool readPdbDirectory(
    const std::vector<std::uint8_t>& bytes,
    std::uint32_t& blockSize,
    std::uint32_t& blockCount,
    std::vector<std::uint8_t>& directory,
    std::string& error)
{
    blockSize = readU32(bytes, 32);
    blockCount = readU32(bytes, 40);
    const std::uint32_t directoryByteCount = readU32(bytes, 44);
    const std::uint32_t blockMapIndex = readU32(bytes, 52);
    if (!validBlockSize(blockSize) || blockCount == 0 ||
        static_cast<std::uint64_t>(blockCount) * blockSize > bytes.size() ||
        directoryByteCount < 4 || directoryByteCount > bytes.size() || blockMapIndex >= blockCount)
    {
        error = "PDB MSF superblock is inconsistent";
        return false;
    }
    const std::uint32_t directoryBlockCount = (directoryByteCount + blockSize - 1U) / blockSize;
    const std::size_t blockMapOffset = static_cast<std::size_t>(blockMapIndex) * blockSize;
    if (!rangeFits(blockMapOffset, static_cast<std::size_t>(directoryBlockCount) * 4, bytes.size()))
    {
        error = "PDB directory block map is truncated";
        return false;
    }
    directory.reserve(directoryByteCount);
    for (std::uint32_t index = 0; index < directoryBlockCount; ++index)
    {
        const std::uint32_t block = readU32(bytes, blockMapOffset + (index * 4U));
        if (block >= blockCount)
        {
            error = "PDB directory references a block outside the MSF file";
            return false;
        }
        const std::size_t sourceOffset = static_cast<std::size_t>(block) * blockSize;
        const std::size_t remaining = directoryByteCount - directory.size();
        const std::size_t copySize = std::min<std::size_t>(blockSize, remaining);
        if (!rangeFits(sourceOffset, copySize, bytes.size()))
        {
            error = "PDB directory block is truncated";
            return false;
        }
        directory.insert(directory.end(), bytes.begin() + sourceOffset, bytes.begin() + sourceOffset + copySize);
    }
    return true;
}

bool parsePdbStreams(
    const std::vector<std::uint8_t>& directory,
    const std::uint32_t blockSize,
    const std::uint32_t blockCount,
    std::vector<std::uint32_t>& streamSizes,
    std::vector<std::vector<std::uint32_t>>& streamBlocks,
    std::string& error)
{
    const std::uint32_t streamCount = readU32(directory, 0);
    if (streamCount < 4 || streamCount > 65536 ||
        !rangeFits(4, static_cast<std::size_t>(streamCount) * 4, directory.size()))
    {
        error = "PDB stream directory has an invalid stream count";
        return false;
    }
    streamSizes.resize(streamCount);
    std::size_t cursor = 4;
    for (std::uint32_t index = 0; index < streamCount; ++index)
    {
        streamSizes[index] = readU32(directory, cursor);
        cursor += 4;
    }
    streamBlocks.resize(streamCount);
    for (std::uint32_t stream = 0; stream < streamCount; ++stream)
    {
        if (streamSizes[stream] == kAbsentStream) continue;
        const std::uint32_t count = (streamSizes[stream] + blockSize - 1U) / blockSize;
        if (!rangeFits(cursor, static_cast<std::size_t>(count) * 4, directory.size()))
        {
            error = "PDB stream block list is truncated";
            return false;
        }
        streamBlocks[stream].reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const std::uint32_t block = readU32(directory, cursor);
            cursor += 4;
            if (block >= blockCount)
            {
                error = "PDB stream references a block outside the MSF file";
                return false;
            }
            streamBlocks[stream].push_back(block);
        }
    }
    return true;
}

std::optional<std::vector<std::uint8_t>> materializePdbStream(
    const std::uint32_t stream,
    const std::uint32_t blockSize,
    const std::vector<std::uint8_t>& bytes,
    const std::vector<std::uint32_t>& streamSizes,
    const std::vector<std::vector<std::uint32_t>>& streamBlocks)
{
    if (stream >= streamSizes.size() || streamSizes[stream] == kAbsentStream) return std::nullopt;
    std::vector<std::uint8_t> result;
    result.reserve(streamSizes[stream]);
    for (const std::uint32_t block : streamBlocks[stream])
    {
        const std::size_t sourceOffset = static_cast<std::size_t>(block) * blockSize;
        const std::size_t remaining = streamSizes[stream] - result.size();
        const std::size_t copySize = std::min<std::size_t>(blockSize, remaining);
        if (!rangeFits(sourceOffset, copySize, bytes.size())) return std::nullopt;
        result.insert(result.end(), bytes.begin() + sourceOffset, bytes.begin() + sourceOffset + copySize);
    }
    return result;
}

bool verifyPdbInformationStream(
    const std::vector<std::uint8_t>& stream,
    PdbInfo& info,
    std::string& error)
{
    if (stream.size() < 28)
    {
        error = "PDB information stream is missing or truncated";
        return false;
    }
    info.version = readU32(stream, 0);
    info.signature = readU32(stream, 4);
    info.age = readU32(stream, 8);
    std::copy_n(stream.begin() + 12, info.guid.size(), info.guid.begin());
    if (info.age == 0)
    {
        error = "PDB information stream has a zero age";
        return false;
    }
    return true;
}

bool verifyPdbDbiStream(
    const std::vector<std::uint8_t>& stream,
    const std::vector<std::uint32_t>& streamSizes,
    PdbInfo& info,
    std::string& error)
{
    if (stream.size() < 64)
    {
        error = "PDB DBI stream is missing or truncated";
        return false;
    }
    if (readU32(stream, 0) != 0xffffffffU)
    {
        error = "PDB DBI stream has an invalid signature";
        return false;
    }
    const std::uint16_t publicStreamIndex = readU16(stream, 16);
    const std::uint32_t moduleInfoSize = readU32(stream, 24);
    const std::uint32_t fileInfoSize = readU32(stream, 36);
    info.hasModuleRecords = moduleInfoSize != 0 && rangeFits(64, moduleInfoSize, stream.size());
    info.hasSourceFiles = fileInfoSize != 0;
    info.hasPublicSymbols = publicStreamIndex < streamSizes.size() &&
        streamSizes[publicStreamIndex] != kAbsentStream && streamSizes[publicStreamIndex] != 0;
    return true;
}
}

bool PdbVerifier::verify(const std::filesystem::path& path)
{
    info_ = {};
    error_.clear();

    std::vector<std::uint8_t> bytes;
    if (!readPdbBytes(path, bytes, error_)) return false;
    std::uint32_t blockSize = 0;
    std::uint32_t blockCount = 0;
    std::vector<std::uint8_t> directory;
    if (!readPdbDirectory(bytes, blockSize, blockCount, directory, error_)) return false;
    std::vector<std::uint32_t> streamSizes;
    std::vector<std::vector<std::uint32_t>> streamBlocks;
    if (!parsePdbStreams(directory, blockSize, blockCount, streamSizes, streamBlocks, error_)) return false;
    info_.streamCount = static_cast<std::uint32_t>(streamSizes.size());
    const auto pdbStream = materializePdbStream(1, blockSize, bytes, streamSizes, streamBlocks);
    if (!pdbStream.has_value())
    {
        error_ = "PDB information stream is missing or truncated";
        return false;
    }
    if (!verifyPdbInformationStream(*pdbStream, info_, error_)) return false;
    const auto dbiStream = materializePdbStream(3, blockSize, bytes, streamSizes, streamBlocks);
    if (!dbiStream.has_value())
    {
        error_ = "PDB DBI stream is missing or truncated";
        return false;
    }
    if (!verifyPdbDbiStream(*dbiStream, streamSizes, info_, error_)) return false;
    for (std::uint32_t stream = 0; stream < streamSizes.size() && !info_.hasSourceChecksums; ++stream)
    {
        if (streamSizes[stream] == kAbsentStream || streamSizes[stream] < 8) continue;
        const auto data = materializePdbStream(stream, blockSize, bytes, streamSizes, streamBlocks);
        if (data.has_value()) info_.hasSourceChecksums = containsChecksumSubsection(*data);
    }
    return true;
}

const PdbInfo& PdbVerifier::info() const noexcept
{
    return info_;
}

const std::string& PdbVerifier::error() const noexcept
{
    return error_;
}
}
