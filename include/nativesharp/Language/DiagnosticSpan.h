#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct DiagnosticSpan final
{
    std::string file;
    std::size_t line = 0;
    std::size_t column = 0;
    std::size_t endLine = 0;
    std::size_t endColumn = 0;
};
}
