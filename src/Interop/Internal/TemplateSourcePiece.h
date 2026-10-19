#pragma once

#include "nativesharp/Basic/SourceRange.h"

#include <cstddef>

namespace nativesharp::interop
{
namespace
{
struct TemplateSourcePiece final
{
    SourceRange range;
    std::size_t outputBegin = 0;
};
}
}
