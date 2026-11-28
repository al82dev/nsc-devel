# CC2 — Lifetime planning for C++ copy construction

## Situation

CC1 lets a local of a non-trivial C++ record be initialized from a place (`T x = place;`): the local is
constructed with the selected copy constructor from the place's address and destroyed once by its scope.

That construction has no construction site of its own (it does not come from `new T(...)`), so it carries the
selected constructor but no construction binding. The C++ lifetime planning over MIR, which collects the
constructors and destructors a translation unit has to emit, required a construction binding for every C++
value construction of a local or a discarded value and rejected the copy with:

```
discarded C++ value has incomplete construction identity
```

## Change

- The planner recognizes the copy shape produced by CC1: no construction binding, one selected constructor,
  exactly one argument, and that argument is the address of a value of the same type. For it, the constructor
  and the destructor are checked the same way as for any other construction (a constructor of the target type,
  one matching destructor), and implicit special members are selected for emission as usual.
- Constructions that are neither a construction site, a factory call nor this copy are still rejected. The
  message now names the type and whether it is a local or a discarded value, and says when no constructor was
  selected:

```
C++ local of type 'T' has incomplete construction identity
```

## Not covered

Copies into fields, arguments or returned values keep their existing paths; this change only concerns the
local copy construction introduced by CC1.
