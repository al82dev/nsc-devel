#pragma once

namespace nativesharp::frontend
{
enum class ImplicitDefaultConstructorResolutionKind
{
    NoBaseRequired,
    ResolvedExplicitBaseEntry,
    ResolvedImplicitBaseChain,
    NoViableBaseConstructor,
    InaccessibleBaseConstructor,
    Invalid
};
}
