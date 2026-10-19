#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct HygienicName final { std::string spelling; std::size_t scopeId = 0; };
}
