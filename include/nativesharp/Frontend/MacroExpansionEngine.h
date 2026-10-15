#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
// NS304-T14-F03: the permitted invocation contexts of a macro/mixin. A macro declares the
// context it expands into; the expander rejects an unknown context keyword before the expanded
// AST is published, so a statement macro can never silently surface as an expression.
enum class MacroExpansionContext { Statement, Expression, Member, Type };

struct MacroExpansionRecord final
{
    std::string macroName;
    std::size_t invocationOffset = 0;
    std::size_t invocationLength = 0;
    std::size_t expansionLength = 0;
    std::size_t hygieneScope = 0;
    MacroExpansionContext context = MacroExpansionContext::Statement;
    // NS304-T14-F04: source mapping of a generated expansion — the template (definition) location
    // that produced it, so a diagnostic can navigate to both the invocation and the template.
    std::size_t definitionOffset = 0;
    std::size_t definitionLength = 0;
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
