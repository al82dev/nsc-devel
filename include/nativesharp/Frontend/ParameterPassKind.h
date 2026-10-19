#pragma once

namespace nativesharp::frontend
{
// C4 "language references". How a parameter receives its argument.
//
// All three by-reference forms are ONE machine-level thing -- a pointer -- and that is deliberate:
// C1 already made `&x`, `*p` and pointer parameters work end to end, so `ref`/`out`/`in` are
// sugar over machinery that is already certified rather than a new calling convention. What
// distinguishes them is entirely a matter of RULES the frontend enforces:
//
//   Ref -- the argument must already hold a value, and the callee may read and write it.
//   Out -- the callee MUST assign it before returning; the caller need not initialize it first.
//   In  -- pass by reference for cost, read-only at the callee. No copy, no writes.
//
// The keyword is required at the CALL SITE too, exactly as in C#, so that a reader of the call
// can see that the argument may change without having to go read the signature.
enum class ParameterPassKind
{
    Value,
    Ref,
    Out,
    In
};
}
