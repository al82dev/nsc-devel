# T20 - Diagnostic for an implicit copy of a non-trivial C++ record

## Problem

A C++ record whose copy or destruction is not trivial is not copied implicitly (`T copy = source;` with
`source` a place). The diagnostic said `use move(source)`. For an element or a field of C++ storage this is the
wrong advice: moving leaves the source in its moved-from state, while the C++ code copies it with the copy
constructor.

## Rule (unchanged)

The copy stays explicit. The copy constructor is called with `new T(in source)`; `move(source)` transfers.

## Change

`src/Frontend/Bodies/StatementAnalyzer.cpp`, `validateVariableOwnershipInitializer`: when the move-only local is a
C++ record, the diagnostic names both forms. Other move-only types keep `use move(source)`.

## Status

Built (Release O3). Message checked on a small project. The suggested copy form itself was not compiled in that
check: pending verification.
