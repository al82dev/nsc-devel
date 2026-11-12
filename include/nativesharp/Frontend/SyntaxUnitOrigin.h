#pragma once

namespace nativesharp::frontend
{
// Provenance of the syntax input, independent from whether its bodies are
// analyzed locally. Unknown must not be treated as original source.
enum class SyntaxUnitOrigin
{
    Unknown,
    Source,
    PrecompiledMetadata
};
}
