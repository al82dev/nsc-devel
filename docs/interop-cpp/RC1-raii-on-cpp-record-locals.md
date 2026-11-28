# RC1 - `raii` on a C++ record local is redundant

## Analysis

A local of C++ record type holds the object itself:

- construction: `T x = new T(...)` (or a factory call, or CC1's copy) builds the object in the local's storage;
- ownership: the local is the only owner; it can be transferred only with `move(x)` (the record is move-only when
  its copy or destruction is not trivial);
- destruction: the C++ lifetime cleanup of the local runs the destructor once at the end of its scope.

`raii` made such a local an owner of a separate allocation, a contract that does not exist for a value: before
T16 this risked a second destruction, and T16 turned it into an error.

None of the three contracts above depends on `raii`, so the keyword can be accepted with no effect: with it or
without it the local is constructed, owned and destroyed exactly the same way, once.

## Rule

`raii T x = ...;` with `T` a C++ record (not a pointer to one) is accepted and is the same declaration as
`T x = ...;`. The keyword is ignored for that local: it does not enable the owner-of-allocation cleanup, so no
second destruction can happen. `owned` and `borrowed` on such a local are not changed by this rule.

## Change

`src/Frontend/Bodies/StatementAnalyzer.cpp`, `variableRequiresRaii`: the `raii` keyword does not make a C++
record value local an owner.

## Status

Built (Release). See the tanda report for the programs compiled against it.
