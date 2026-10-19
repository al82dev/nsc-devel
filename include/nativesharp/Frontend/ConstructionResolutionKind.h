#pragma once

namespace nativesharp::frontend
{
enum class ConstructionResolutionKind
{
    ResolvedExplicit,
    ResolvedImplicitDefault,
    NoViableCandidate,
    Ambiguous,
    Inaccessible,
    InvalidTarget
};
}
