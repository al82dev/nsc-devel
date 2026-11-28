# A1 - Inferred owner for a local initialized by an owned transfer

## Problem

`T x = f();` and `static T x = f();`, with `f` returning `owned T` (a class reference, a string, a delegate), were
rejected: "owned return value cannot initialize a non-owning local; use a raii owner destination". In C and C++
the variable an owned result initializes is the owner of that result (`std::string root = make();`,
`static std::string root = make();`). NativeSharp already applies this to `T x = new T()` (item 141) and to C++
factory calls of non-trivial records (X2-05), but not to owned-returning calls.

## Rule

An ordinary local without an ownership qualifier, initialized directly by an owned transfer, is an inferred owner
(the same behavior as `raii`):

- destroyed at the end of its scope; a static local is destroyed at process exit (L-014);
- moved only with `move(...)`; copying it into another owner stays rejected;
- `return x;` from a method whose result is not owned is a borrow only for a static local (L-014); a returned
  owned result still needs `return move(x);`.

Excluded, keeping the existing diagnostic: reference locals (`ref`, pinned, foreach and pattern references),
`scope` locals, compiler locals (`$...`), borrowed locals and locals that already carry an ownership qualifier.
Assignments after the declaration remain non-owning, as for item 141.

No valid program changes meaning: only declarations that were errors become owners.

## Change

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: member `inferredOwnerFromOwnedInitializer_`.
- `src/Frontend/Bodies/StatementAnalyzer.cpp`:
  - `validateVariableOwnershipInitializer` sets the flag instead of reporting the error for an ordinary local;
  - `finishVariableAnalysis` resets the flag before the initializer and, after it, turns the local into an owner
    (`isRaii`) before the local is registered, so its cleanup region and static destruction are created as for
    `raii`.

## Impact

- Diagnostics that named this case now accept it; tests that expected the old error need their expectation
  updated (not run).
- Native arrays are not affected: an unqualified `T[]` local is not an ownable destination here.

## Status

Implemented with static review only. Not built, not compiled against any program: pending build and verification.
