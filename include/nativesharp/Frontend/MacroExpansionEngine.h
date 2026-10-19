#pragma once

#include "nativesharp/Frontend/MacroExpansionContext.h"
#include "nativesharp/Frontend/MacroExpansionRecord.h"
#include "nativesharp/Frontend/MacroExpansionResult.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// Expands source-level syntax macros before lexing. Parameters are referenced as `$name` and
// introduced bindings as `$#name`; the latter receive a deterministic per-expansion spelling.
class MacroExpansionEngine final
{
public:
    // NS304-T14-F07: `maximumExpansions` bounds the total expansion steps; `maximumDepth` bounds
    // the nesting depth separately, so a cyclic macro fails with a specific cycle diagnostic
    // instead of being absorbed generically by the step budget.
    explicit MacroExpansionEngine(
        std::size_t maximumExpansions = 64,
        std::size_t maximumDepth = 32) noexcept;
    MacroExpansionResult expand(std::string_view source) const;

private:
    std::size_t maximumExpansions_ = 64;
    std::size_t maximumDepth_ = 32;
};

// NS304-T14-F04: renders the expansion chain with source mapping (invocation + template offsets)
// for a diagnostic backtrace without fictitious spans.
std::string macroExpansionBacktrace(std::span<const MacroExpansionRecord> expansions);
}
