#pragma once

#include <filesystem>

namespace nativesharp::llvm_backend
{
struct DebugPrefixMap final
{
    std::filesystem::path sourcePrefix;
    std::filesystem::path replacementPrefix;
};
}
