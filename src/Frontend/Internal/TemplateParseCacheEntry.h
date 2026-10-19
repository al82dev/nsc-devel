#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace nativesharp::frontend::internal
{
struct TemplateParseCacheEntry final
{
    std::string key;
    std::filesystem::path payload;
    std::filesystem::path metadata;
    std::filesystem::file_time_type publication;
    std::uintmax_t size;
};
}
