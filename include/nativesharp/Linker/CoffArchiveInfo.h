#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::linker
{
struct CoffArchiveInfo final
{
    std::size_t memberCount = 0;
    std::vector<std::string> symbolNames;
};
}
