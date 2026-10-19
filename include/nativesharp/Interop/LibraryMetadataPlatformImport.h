#pragma once

#include <string>

namespace nativesharp::interop
{
struct LibraryMetadataPlatformImport final
{
    std::string dllName;
    std::string symbolName;
};
}
