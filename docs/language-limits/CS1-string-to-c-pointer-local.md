# CS1 - Runtime string as the initializer of a read-only C character pointer local

## Capability

```
sbyte const* p = text;      // text: a runtime string
byte const* q = text;
```

A read-only 8-bit C character pointer local can be initialized from a runtime string, for any C or C++ API that
takes a NUL-terminated string through a local (an array of pointers, a pointer to a pointer, a pointer kept for
several calls). Before this change only a direct C call argument converted a runtime string (L-003), and a string
literal pointed at its static bytes (L-078).

## Contract

| Question | Rule |
|---|---|
| Representation | A copy of the string's UTF-8 bytes followed by one NUL byte, `length + 1` bytes, made once when the declaration runs. A NativeSharp string has no terminator, so the pointer can never point into the string itself. Interior NUL bytes are copied as they are. |
| Owner | The pointer local. The copy is storage of the declaring stack frame, like a variable-length array; it is not a heap allocation and needs no release call. |
| Lifetime | From the declaration to the end of the frame. A declaration inside a loop body gets a new copy each iteration and the previous one is released when the next iteration starts (the loop stack scope used for VLAs). The copy never ends before the local's scope. |
| Release | Automatic, with the frame or the loop iteration (stack restore). Nothing to free, no destructor, no double release. |
| Null string | The pointer is null; no copy is made. |
| Mutability | The pointee must be `const`. `sbyte* p = text;` is rejected: the copy is never writable, as a literal's bytes are not. |
| Character type | `sbyte const*` and `byte const*` only (UTF-8 bytes). `char const*` is rejected. |
| Snapshot | Later changes to the string variable (a new string assigned to it) do not change the copy. |

## What keeps the pointer from outliving the copy

The local is the root of its own frame storage (`variableLengthArrayRootLocalOrdinal` = itself), so the checks that
already protect VLA storage apply unchanged:

- `return p;` (and any pointer derived from it) is rejected: the frame ends with the call;
- storing `p` (or a derived pointer) anywhere other than a local alias (a field, a static, through a pointer, an
  `out` parameter) is rejected;
- capturing `p` in a lambda is rejected;
- a local alias (`sbyte const* r = p;`) inherits the root and the same checks.

Passing `p`, `&p` or a derived pointer to a call is allowed: the call runs inside the frame. A C or C++ function
that keeps the pointer after it returns is outside what the compiler can check, as for any pointer to frame
storage.

## Rejected forms

| Form | Reason |
|---|---|
| `p = text;` (assignment) | Only the declaration makes the copy, so the copy's lifetime is exactly the local's. |
| `static sbyte const* p = text;` | A static has module lifetime; the copy is frame storage. |
| declaration in an async or iterator callable | The frame is rebuilt across suspensions; stack storage does not survive them. |
| `sbyte* p = text;`, `char const* p = text;` | The copy is read-only bytes. |
| field, namespace state, return of a string as `sbyte const*` | No local owns the copy; the existing conversion errors stay. |

## Changes

- `include/nativesharp/Frontend/MethodLocalSymbol.h`: `ownsCStringCopy`.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: `pendingLocalCStringCopy_`.
- `src/Frontend/Bodies/StatementAnalyzer.cpp`:
  - `validateVariableInitializerConversion` accepts the form and reports the rejected forms with their reason;
  - `finishVariableAnalysis` marks the registered local (`ownsCStringCopy`, VLA root = itself) or rejects static
    locals and suspending callables;
  - `rejectVariableArrayReturn` names the copy in its message.
- `src/Frontend/Bodies/AssignmentAnalyzer.cpp`: assignment diagnostic; store-outside message names the copy.
- `src/Frontend/Bodies/LambdaDelegateBinder.cpp`: capture message names the copy.
- `src/Frontend/HIR/ExpressionStatementMachine/HirDeclarationFrameLowering.cpp`: the initializer of such a local is
  wrapped in the L-003 `StringToCharPointer` conversion.
- `src/LLVM/Lowering/OopLlvmExpressionMachine.cpp`: `buildCStringStackCopy` (the copy, shared with L-003 call
  arguments); a `StringToCharPointer` conversion that no call claims makes the copy in the frame instead of failing.
- `src/LLVM/Lowering/OopLlvmControlFlow.cpp`: a loop whose body makes such a copy restores the stack each iteration.

## Pending verification

- Not run against any program yet.
- An initializer that is an owned temporary string (a call returning `owned string`): the copy is made before the
  temporary is released; the release of that temporary on this path has not been checked.
- A loop inside a `catch` handler does not restore the stack per iteration (same as VLAs).
- Existing unit tests that expected the old diagnostic for this form need their expectation updated (not run).
