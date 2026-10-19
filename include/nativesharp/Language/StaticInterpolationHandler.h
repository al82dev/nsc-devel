#pragma once

#include <string>
#include <string_view>

#include "nativesharp/Language/InterpolationPart.h"

namespace nativesharp::language
{
// T01-F06. Interpolation parsing and handler selection are deliberately separate. The parser
// freezes literal/hole order; a compile-time handler decides which hole spellings it accepts and
// appends their representation. The handler is supplied by reference, so selecting a custom
// handler does not imply target-side allocation or a dependency on NativeSharp.Core.String.
class StaticInterpolationHandler
{
public:
    virtual ~StaticInterpolationHandler() = default;
    virtual bool appendLiteral(std::string_view text, std::string& error) = 0;
    virtual bool appendHole(const InterpolationPart& hole, std::string& error) = 0;
};
}
