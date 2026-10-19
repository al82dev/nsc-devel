#pragma once

#include <cstddef>
#include <filesystem>

namespace nativesharp::compilation
{
struct ProjectSource final
{
    std::filesystem::path absolutePath;
    std::filesystem::path projectRelativePath;
    std::size_t ordinal = 0;
};
}
