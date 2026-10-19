#pragma once

namespace nativesharp::frontend
{
// NS304-T14-F03: the permitted invocation contexts of a macro/mixin. A macro declares the
// context it expands into; the expander rejects an unknown context keyword before the expanded
// AST is published, so a statement macro can never silently surface as an expression.
enum class MacroExpansionContext { Statement, Expression, Member, Type };
}
