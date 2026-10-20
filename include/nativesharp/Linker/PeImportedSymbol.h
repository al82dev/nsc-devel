#pragma once

#include <string>

namespace nativesharp::linker
{
struct PeImportedSymbol final
{
    std::string library;
    std::string name;
};
}
