#pragma once

#include <cstddef>
#include <string>

#include "nativesharp/Language/InterpolationPartKind.h"

namespace nativesharp::language
{
struct InterpolationPart final
{
    InterpolationPartKind kind = InterpolationPartKind::Literal;
    std::string text;
    std::string format;
    // Byte position of the trimmed hole expression inside the decoded interpolation template.
    // Literal parts leave these at zero. The parser uses this only to keep diagnostics for a
    // reparsed runtime hole close to the source spelling; semantic identity never depends on it.
    std::size_t expressionOffset = 0;
    std::size_t expressionLength = 0;
};
}
