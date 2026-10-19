#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::interop
{
namespace
{
struct TemplateSourceReplacement final
{
    std::size_t begin = 0;
    std::size_t end = 0;
    std::string text;
};
}
}
