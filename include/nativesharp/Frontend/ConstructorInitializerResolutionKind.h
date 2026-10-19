#pragma once

namespace nativesharp::frontend
{
enum class ConstructorInitializerResolutionKind
{
    NoChainingRequired,
    ResolvedExplicit,
    ResolvedImplicitDefault,
    NoViableCandidate,
    Ambiguous,
    Inaccessible,
    Invalid
};
}
