#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
struct ComptimeEvaluation final
{
    std::size_t offset = 0;
    std::size_t length = 0;
    std::int64_t value = 0;
};

struct ComptimeResult final
{
    bool success = false;
    std::string source;
    std::string error;
    std::vector<ComptimeEvaluation> evaluations;
};

// Executes the deliberately small NativeSharp 2.0 comptime core. `comptime` is a contextual
// marker and `#run(integer-expression)` is replaced with its signed 64-bit result before lexing.
// Replacements retain the original byte count, so every later source range still names the
// original file offset. The engine never invokes native code or the host environment.
class ComptimeEngine final
{
public:
    ComptimeResult execute(std::string_view source) const;
};
}
