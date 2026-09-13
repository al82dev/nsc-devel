#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <span>
#include <string>

namespace nativesharp::interop
{
// Combines independently collected NSMETA fragments into one canonical model.
// The merge is independent of worker completion order: record identity controls
// publication order, exact duplicates collapse, and conflicting duplicates fail.
class LibraryMetadataMerger final
{
public:
    bool merge(
        std::span<const LibraryMetadata> fragments,
        LibraryMetadata& merged,
        std::string& error) const;
};
}
