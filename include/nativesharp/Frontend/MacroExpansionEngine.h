#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
struct MacroExpansionRecord final
{
    std::string macroName;
    std::size_t invocationOffset = 0;
    std::size_t invocationLength = 0;
    std::size_t expansionLength = 0;
    std::size_t hygieneScope = 0;
};

struct MacroExpansionResult final
{
    bool success = false;
    std::string source;
    std::string error;
    std::vector<MacroExpansionRecord> expansions;
};

// Expands source-level syntax macros before lexing. Parameters are referenced as `$name` and
// introduced bindings as `$#name`; the latter receive a deterministic per-expansion spelling.
class MacroExpansionEngine final
{
public:
    explicit MacroExpansionEngine(std::size_t maximumExpansions = 64) noexcept;
    MacroExpansionResult expand(std::string_view source) const;

private:
    std::size_t maximumExpansions_ = 64;
};
}
