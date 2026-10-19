#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace nativesharp::language_service
{
struct StoredDocument final
{
    std::filesystem::path path;
    std::string text;
    std::int64_t version = 0;
};
}
