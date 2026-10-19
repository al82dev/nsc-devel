#pragma once

namespace nativesharp::frontend
{
// NS304-T13-F08. Explicit allocation-failure profile for `new (allocator, failure: ...) T(...)`.
enum class ProjectAllocatorFailureProfile
{
    None,    // no `failure:` — preserves T13-F02/T13-F07 behaviour.
    Null,    // `failure: null`    — Allocate null -> construction result is null.
    Result,  // `failure: result`  — Result<owned T, AllocationFailure>.
    Throws,  // `failure: throws`  — Allocate null -> AllocationFailureException.
    Panic    // `failure: panic`   — Allocate null -> project panic profile.
};
}
