# CC1 - Copy initialization of a non-trivial C++ record local

## Problem

`T x = place;`, with `T` a C++ record whose copy or destruction is not trivial and `place` an existing object of
`T` (a local, a field, an element, the pointee of a pointer), was rejected: such a record is move-only in
NativeSharp and the diagnostic only offered `move(place)`. C++ initializes `x` with the copy constructor and leaves
`place` untouched.

## Rule

`T x = place;` copy-constructs `x` in its own storage when C++ allows the copy:

| Source | Constructor used |
|---|---|
| modifiable place and `T(T&)` declared | `T(T&)` |
| otherwise | `T(const T&)` |

The constructor must be a C++ constructor of `T`, not deleted, not a factory constructor, and public. The
place is passed by address; it is neither moved nor destroyed. `x` is an ordinary C++ record local: constructed
once, destroyed once at the end of its scope (the same cleanup as `T x = new T(...)`).

When no constructor qualifies the declaration stays an error and the diagnostic gives the reason:

- the record declares no copy constructor;
- the copy constructor is deleted;
- the copy constructor is not public;
- only `T(T&)` exists and the source is read-only.

`move(place)` keeps its meaning (transfer).

## Not changed

- Assignment to an existing local (`x = place;`) keeps C++ assignment (`operator=`).
- Passing a non-trivial C++ record by value to a NativeSharp function stays rejected (X2-05).
- A static local copy-initialized this way is rejected by the C++ lifetime check (static C++ record locals are
  only constructed directly).

## Changes

- `include/nativesharp/Frontend/MethodLocalSymbol.h`: `cppCopyConstructorOrdinal`.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: `CppCopyConstructorSelection`,
  `selectCppCopyConstructor`, `pendingCppCopyConstructor_`.
- `src/Frontend/Bodies/StatementAnalyzer.cpp`: selection, diagnostics, recording on the local.
- `src/Frontend/HIR/ExpressionStatementMachine/HirDeclarationFrameLowering.cpp`: the initializer is lowered as an
  address and the declaration's value is `ValueConstruction(copy constructor, AddressValue(place))`.

## Status

Built (Release). See the tanda report for the programs compiled against it.
