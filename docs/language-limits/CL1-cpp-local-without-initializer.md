# CL1 — C++ record local without an initializer

## Situation

A local of an imported C++ record whose copy or destruction is not trivial is tracked by the C++ local
lifetime rules: its cleanup is bound to the construction that creates it, so the declaration must construct
it directly (`T value = new T(...);`, a call that returns a `T`, or a copy from a value).

A declaration without an initializer (`T value;`) has no construction to bind that cleanup to. It used to be
rejected with a generic message ("experimental C++ local requires direct automatic construction and
cleanup") that did not say what to write instead.

## Rule

The rule itself does not change. As with item 10.1 (a C++ type with a non-trivial default constructor), a
default-initialized local of such a type is not constructed implicitly: NativeSharp keeps explicit
construction and definite assignment. The diagnostic now names the type and the accepted forms:

```
C++ type 'T' has a non-trivial copy or destruction; initialize the local with 'new T()' or with a value
```

## Accepted forms

```
T value = new T();          // default constructor, destroyed at the end of the scope
T value = new T(arguments); // any accessible constructor
T value = MakeT();          // a call that constructs the value in place
```

## Not covered

Implicit default construction of `T value;`, as C++ does, would change the language rules for definite
assignment and explicit construction, and is not part of this change.
