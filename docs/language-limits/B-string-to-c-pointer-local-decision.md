# B - Runtime string stored in a C character pointer local (decision record)

Superseded by CS1 (`CS1-string-to-c-pointer-local.md`): the capability is implemented with frame storage, not with
a hidden heap allocation. The record below is kept as the earlier decision.

## Question

Should `sbyte const* p = text;` (a runtime string into a C character pointer local) be accepted, with a hidden
NUL-terminated copy owned by the local?

## Facts

- A NativeSharp string is never NUL-terminated (layout: length, then bytes), so a pointer cannot borrow its
  storage the way C++ `c_str()` borrows a `std::string` buffer.
- L-003 converts a string to `const char*` only as a direct C call argument, with a stack copy that lives for the
  call.
- C++ has no implicit `std::string` to `const char*` conversion; it requires `c_str()`.
- `NativeSharp.Core.CString` already exists: `new CString(text)` keeps a NUL-terminated copy, `Pointer()` returns
  its address (`byte*`), `Length` and `SizeWithNul` describe it.

## Decision taken here

No implicit conversion is added. The form compatible with C++ and with NativeSharp's explicit ownership is the
explicit one:

    raii CString vertexText = new CString(vertexCode);
    sbyte const* vertexSource = (sbyte const*)vertexText.Pointer();

The owner decides the lifetime, as `std::string` decides the lifetime of `c_str()`. T19 makes the diagnostic name
this form.

## Core note

`CString.Pointer()` returns `byte*`. C `char` is `sbyte` on the supported targets, so a `sbyte const*` accessor
(the equivalent of `c_str()`) would avoid the cast. That is a Core API addition, not a compiler change.

## Rejected alternative

A hidden heap copy owned by the pointer local (released at scope exit, escape rejected). It would add an
allocation that the program does not show and a pointer local with ownership, which no other pointer has.

The example above has not been compiled.
