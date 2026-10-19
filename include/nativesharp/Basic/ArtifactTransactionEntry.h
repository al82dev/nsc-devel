#pragma once

#include <filesystem>

namespace nativesharp::basic
{
struct ArtifactTransactionEntry final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
};
}
