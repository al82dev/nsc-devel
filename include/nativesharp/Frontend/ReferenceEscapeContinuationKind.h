#pragma once

namespace nativesharp::frontend
{
enum class ReferenceEscapeContinuationKind
{
    MatchArms,
    CallArguments,
    PointerPointee
};
}
