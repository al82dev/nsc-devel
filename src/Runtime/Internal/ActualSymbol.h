#pragma once

#include "nativesharp/Runtime/RuntimeAbiSymbolKind.h"

#include <string>

namespace nativesharp::runtime
{
namespace
{
struct ActualSymbol
{
    RuntimeAbiSymbolKind kind;
    std::string llvmType;
};
}
}
