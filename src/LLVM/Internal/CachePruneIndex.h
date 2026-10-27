#pragma once

#include "CachePruneIndexRecord.h"
#include "CachePruneIndexSnapshot.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
// Persistent list of the artifact-cache entries prune can evict (root/prune-index), so prune does
// not have to walk objects/ and partitions/ on every invocation.
//
// Text format, one record per line:
//   NSCPRUNEINDEX 1 <generation> <records> <lines since walk>   header, written by rewrite only
//   + <o|p> <key> <publication ticks> <size>                     an entry was published
//   - <o|p> <key>                                                an entry was removed
//   !                                                            stale: walk the folders next time
// Publications append lines under root/prune-index.lock; prune rewrites the file atomically. Any
// unreadable content makes prune walk the folders and regenerate the file, so the index only
// saves work and never decides what a walk would not.
class CachePruneIndex final
{
public:
    // Lines appended since the last folder walk after which prune walks again, so entries removed
    // by hand (or appends that failed) are reconciled.
    static constexpr std::uintmax_t kReconciliationLines = 4096;

    explicit CachePruneIndex(const std::filesystem::path& root);

    // Records the publication of one entry.
    bool appendPublication(const CachePruneIndexRecord& record, std::string& error) const;

    // Records the removal of one entry (quarantine).
    bool appendRemoval(char kind, std::string_view key, std::string& error) const;

    // Makes the next prune walk the folders and regenerate the index.
    bool markStale(std::string& error) const;

    // Reads the index under its lock. A missing or unusable index is not an error: the snapshot
    // is just not valid.
    bool read(CachePruneIndexSnapshot& snapshot, std::string& error) const;

    // Replaces the index, under its lock, with `kept` plus the lines other processes appended
    // after `snapshot` was read. `walked` tells whether `kept` came from a folder walk (which
    // resets the reconciliation counter). Leaves the file alone when another prune rewrote it
    // after `snapshot` was read.
    bool rewrite(
        const CachePruneIndexSnapshot& snapshot,
        std::span<const CachePruneIndexRecord> kept,
        bool walked,
        std::string& error) const;

private:
    bool appendLine(std::string_view line, std::string& error) const;

    std::filesystem::path path_;
    std::filesystem::path lockPath_;
};
}
