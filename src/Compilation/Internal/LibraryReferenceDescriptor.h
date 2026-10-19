#pragma once

#include <optional>
#include <string>

namespace nativesharp::compilation
{
struct LibraryReferenceDescriptor final
{
    std::string include;
    std::optional<std::string> metadata;
    std::optional<std::string> runtime;
};
}
