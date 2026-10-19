#pragma once

namespace nativesharp::ast
{
// T14-F04. Source-level floating-point policy is an optimization/semantic contract, not a
// physical type property. Inherit means the command-line/project policy remains authoritative.
enum class FloatingPointModeSyntaxKind
{
    Inherit,
    Strict,
    Precise,
    Fast
};
}
