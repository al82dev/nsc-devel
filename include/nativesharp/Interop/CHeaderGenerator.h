#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::interop
{
class CHeaderGenerator final
{
public:
    bool writeHeader(
        const frontend::ProjectCompilation& compilation,
        const std::filesystem::path& destination,
        std::string& error) const;
};
}
