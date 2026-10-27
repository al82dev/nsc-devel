#include "CachePruneIndex.h"

#include "ArtifactCacheFilesystemPath.h"
#include "CacheEntryLock.h"
#include "CachePruneIndexHeader.h"
#include "CachePruneRecordTable.h"

#include "../../Frontend/Cache/CacheEntryStore.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr std::string_view kHeaderTag = "NSCPRUNEINDEX";
constexpr std::string_view kHeaderVersion = "1";

std::atomic<unsigned long long> generationOrdinal = 0;
// Only temporarySibling and replaceFileAtomically are used: the metadata header is not.
frontend::internal::CacheEntryStore indexFileStore(
    generationOrdinal, "NSCPRUNEINDEX 1", "prune-index", "artifact-cache prune index", false, true);

/// Reads the whole file into `text`; false when it does not exist or cannot be read.
bool readWholeFile(const std::filesystem::path& path, std::string& text)
{
    std::ifstream stream(cacheFilesystemPath(path), std::ios::binary);
    if (!stream) return false;
    text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return !stream.bad();
}

/// Splits `text` on single spaces.
std::vector<std::string_view> splitFields(std::string_view text)
{
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (start <= text.size())
    {
        const std::size_t space = text.find(' ', start);
        const std::size_t end = space == std::string_view::npos ? text.size() : space;
        fields.push_back(text.substr(start, end - start));
        if (space == std::string_view::npos) break;
        start = space + 1;
    }
    return fields;
}

/// Parses a whole decimal field; false on any other content.
template <typename Number>
bool parseNumber(std::string_view text, Number& value)
{
    if (text.empty()) return false;
    const auto [end, parseError] = std::from_chars(text.data(), text.data() + text.size(), value);
    return parseError == std::errc{} && end == text.data() + text.size();
}

/// Parses the header line; false when it is missing or of another version.
bool parseHeader(std::string_view line, CachePruneIndexHeader& header)
{
    const std::vector<std::string_view> fields = splitFields(line);
    if (fields.size() != 5U || fields[0] != kHeaderTag || fields[1] != kHeaderVersion ||
        fields[2].empty())
        return false;
    header.generation = std::string(fields[2]);
    return parseNumber(fields[3], header.records) && parseNumber(fields[4], header.linesSinceWalk);
}

/// Applies the record lines of `text` from `offset` to `table`. Returns false on a malformed or
/// incomplete line; `lines` counts the lines applied and `stale` tells whether a stale mark was
/// found.
bool applyLines(
    std::string_view text,
    std::size_t offset,
    CachePruneRecordTable& table,
    std::uintmax_t& lines,
    bool& stale)
{
    lines = 0;
    stale = false;
    while (offset < text.size())
    {
        const std::size_t newline = text.find('\n', offset);
        if (newline == std::string_view::npos) return false;
        const std::string_view line = text.substr(offset, newline - offset);
        offset = newline + 1;
        ++lines;
        if (line == "!")
        {
            stale = true;
            continue;
        }
        const std::vector<std::string_view> fields = splitFields(line);
        if (fields.size() < 3U || fields[1].size() != 1U ||
            (fields[1][0] != 'o' && fields[1][0] != 'p') ||
            !frontend::internal::CacheEntryStore::validKey(fields[2]))
            return false;
        if (fields[0] == "-" && fields.size() == 3U)
        {
            table.remove(fields[1][0], fields[2]);
            continue;
        }
        if (fields[0] != "+" || fields.size() != 5U) return false;
        CachePruneIndexRecord record;
        record.kind = fields[1][0];
        record.key = std::string(fields[2]);
        std::filesystem::file_time_type::rep ticks = 0;
        if (!parseNumber(fields[3], ticks) || !parseNumber(fields[4], record.size)) return false;
        record.publication = std::filesystem::file_time_type(
            std::filesystem::file_time_type::duration(ticks));
        table.publish(record);
    }
    return true;
}

/// One publication line, newline included.
std::string publicationLine(const CachePruneIndexRecord& record)
{
    return "+ " + std::string(1, record.kind) + " " + record.key + " " +
        std::to_string(record.publication.time_since_epoch().count()) + " " +
        std::to_string(record.size) + "\n";
}

/// A token that differs between rewrites, also between processes.
std::string newGeneration()
{
    return std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
        std::to_string(generationOrdinal.fetch_add(1, std::memory_order_relaxed));
}
}

CachePruneIndex::CachePruneIndex(const std::filesystem::path& root)
    : path_(root / "prune-index"), lockPath_(root / "prune-index.lock")
{
}

bool CachePruneIndex::appendPublication(
    const CachePruneIndexRecord& record,
    std::string& error) const
{
    return appendLine(publicationLine(record), error);
}

bool CachePruneIndex::appendRemoval(char kind, std::string_view key, std::string& error) const
{
    return appendLine("- " + std::string(1, kind) + " " + std::string(key) + "\n", error);
}

bool CachePruneIndex::markStale(std::string& error) const
{
    return appendLine("!\n", error);
}

bool CachePruneIndex::appendLine(std::string_view line, std::string& error) const
{
    CacheEntryLock lock;
    if (!lock.acquire(lockPath_, error)) return false;
    std::ofstream stream(cacheFilesystemPath(path_), std::ios::binary | std::ios::app);
    if (!stream)
    {
        error = "cannot open artifact-cache prune index '" + path_.string() + "'";
        return false;
    }
    stream.write(line.data(), static_cast<std::streamsize>(line.size()));
    stream.flush();
    if (stream) return true;
    error = "cannot append to artifact-cache prune index '" + path_.string() + "'";
    return false;
}

bool CachePruneIndex::read(CachePruneIndexSnapshot& snapshot, std::string& error) const
{
    snapshot = CachePruneIndexSnapshot{};
    CacheEntryLock lock;
    if (!lock.acquire(lockPath_, error)) return false;
    std::string text;
    if (!readWholeFile(path_, text)) return true;
    snapshot.consumedBytes = text.size();
    const std::size_t newline = text.find('\n');
    CachePruneIndexHeader header;
    if (newline == std::string::npos || !parseHeader(std::string_view(text).substr(0, newline), header))
        return true;
    snapshot.generation = header.generation;
    CachePruneRecordTable table(std::span<const CachePruneIndexRecord>{});
    std::uintmax_t lines = 0;
    bool stale = false;
    if (!applyLines(text, newline + 1, table, lines, stale) || stale || lines < header.records)
        return true;
    snapshot.linesSinceWalk = header.linesSinceWalk + (lines - header.records);
    if (snapshot.linesSinceWalk > kReconciliationLines) return true;
    snapshot.records = table.take();
    snapshot.valid = true;
    return true;
}

bool CachePruneIndex::rewrite(
    const CachePruneIndexSnapshot& snapshot,
    std::span<const CachePruneIndexRecord> kept,
    bool walked,
    std::string& error) const
{
    CacheEntryLock lock;
    if (!lock.acquire(lockPath_, error)) return false;
    std::string text;
    const bool exists = readWholeFile(path_, text);
    std::string generation;
    const std::size_t newline = text.find('\n');
    CachePruneIndexHeader header;
    if (exists && newline != std::string::npos &&
        parseHeader(std::string_view(text).substr(0, newline), header))
        generation = header.generation;
    // Another prune replaced the file after the snapshot: its content is newer than ours.
    if (generation != snapshot.generation || text.size() < snapshot.consumedBytes) return true;
    CachePruneRecordTable table(kept);
    std::uintmax_t appended = 0;
    bool stale = false;
    const bool complete = applyLines(
        text, static_cast<std::size_t>(snapshot.consumedBytes), table, appended, stale);
    const std::vector<CachePruneIndexRecord> records = table.take();
    const std::uintmax_t linesSinceWalk = (walked ? 0U : snapshot.linesSinceWalk) + appended;
    std::string output = std::string(kHeaderTag) + " " + std::string(kHeaderVersion) + " " +
        newGeneration() + " " + std::to_string(records.size()) + " " +
        std::to_string(linesSinceWalk) + "\n";
    for (const CachePruneIndexRecord& record : records) output += publicationLine(record);
    // Lines appended after the snapshot that cannot be read make the next prune walk again.
    if (!complete || stale) output += "!\n";
    const std::filesystem::path temporary = indexFileStore.temporarySibling(path_);
    {
        std::ofstream stream(cacheFilesystemPath(temporary), std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open artifact-cache prune index temporary '" + temporary.string() + "'";
            return false;
        }
        stream.write(output.data(), static_cast<std::streamsize>(output.size()));
        if (!stream)
        {
            stream.close();
            std::error_code ignored;
            std::filesystem::remove(cacheFilesystemPath(temporary), ignored);
            error = "cannot write artifact-cache prune index '" + temporary.string() + "'";
            return false;
        }
    }
    return indexFileStore.replaceFileAtomically(temporary, path_, error);
}
}
