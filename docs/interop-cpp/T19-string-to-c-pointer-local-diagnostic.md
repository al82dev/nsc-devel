# T19 - Diagnostic for a runtime string stored in a C character pointer local

## Problem

`sbyte const* p = text;` with `text` a runtime string reported only
`cannot initialize local 'p' of type 'builtin:sbyte*' from 'builtin:string'`. The message dropped the declared
`const` and did not say why: a runtime string converts to a C character pointer only as a direct argument of a
C call (L-003), where a NUL-terminated copy is made for that call. A NativeSharp string is never NUL-terminated
(length + bytes), so a pointer local cannot borrow its storage, and the per-call copy would not outlive the call.

## Rule (unchanged)

L-003 stays as it is. C++ has no implicit `std::string` to `const char*` conversion either; it asks for
`c_str()`. The NativeSharp equivalent is explicit: `NativeSharp.Core.CString` keeps a NUL-terminated copy with an
owner, and `Pointer()` gives its address for as long as the owner lives.

## Change

`src/Frontend/Bodies/StatementAnalyzer.cpp`, `validateVariableInitializerConversion`: for a `builtin:string`
initializer and a `sbyte*`, `byte*` or `char*` local, the diagnostic spells the declared type with its pointee
`const`, states the rule and names the explicit form (`raii CString c = new CString(text);`, `c.Pointer()`).

## Status

Built (Release O3). Message checked on a small project; no other behavior changes. The suggested text was edited
after that build; the last wording is not built.

## Superseded

CS1 (`docs/language-limits/CS1-string-to-c-pointer-local.md`) accepts `sbyte const* p = text;` and
`byte const* p = text;`. The diagnostic remains for the forms CS1 rejects (writable pointee, `char` pointee) and
states the reason; it no longer suggests an explicit Core type.
