#pragma once

#include <filesystem>

namespace nativesharp::basic::detail
{
struct ArtifactCommitState final
{
    std::filesystem::path sibling;
    std::filesystem::path backup;
    bool hadDestination = false;
    bool backedUp = false;
    bool installed = false;
};
}
