# T18 - Namespace state initialized by construction, not by assignment

## Problem

A mutable namespace state with a non-constant initializer (L-023) is initialized dynamically: the initializer is
turned into the statement `state = initializer;` at the start of the entry point. When the state is a
non-trivial C++ record and the initializer is a construction `new T(...)`, this statement went through C++
assignment (`operator=`) on storage that holds no object yet:

- for an assignable type this is undefined behavior (operator= runs on an object never constructed);
- for a type whose `operator=` is deleted or inaccessible the program was rejected, although C++ itself
  initializes such a namespace-scope variable by construction.

## Rule

The dynamic initializer of a namespace state initializes its storage. When the initializer is a construction
`new T(...)` of a C++ record, it is constructed directly in the state's storage, the same way a store of a
construction of a non-trivial C++ record is lowered as construction in its destination (X2-06). Other
initializers (calls, copies of existing values) keep their current meaning.

## Change

- `src/Frontend/ProjectNamespaceFunctionModel.cpp`, `injectDynamicNamespaceInitializers`: the injected
  assignment is marked with the auxiliary text `namespace-state-initializer`.
- `src/Frontend/Bodies/AssignmentAnalyzer.cpp`, `tryCppAssignment`: a marked store whose right side is a
  non-addressable `ObjectCreationExpression` does not select C++ assignment; it falls through to the ordinary
  store, which lowers as construction in place.

## Pending

- Destruction of such namespace states at process exit is not implemented. A C++ record state constructed this
  way is not destroyed when the program ends. This must be defined (order: reverse of initialization, as C++
  does for namespace-scope objects) and implemented separately.
- Initializers that are not a direct construction (for example a call returning a record) still use the
  previous path.

## Status

Implemented and built (Release O3). Behavior pending verification.
