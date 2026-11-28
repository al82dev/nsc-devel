# RT1 - MSVC runtime support for C libraries built with /MD (analysis, not implemented)

## Problem

An executable links with `/nodefaultlib` and NativeSharp's own entry point, so the default libraries named in the
objects of a referenced C or C++ static library (`/DEFAULTLIB:msvcrt.lib`, `oldnames.lib`) are not linked. The
compiler links runtime import libraries from the toolset and SDK that a reference's system include directories
identify: `ucrt.lib` for any reference, and `msvcprt.lib` + `vcruntime.lib` only for C++ references.

A C static library compiled against the dynamic MSVC runtime (`/MD`, the default of MSVC and clang-cl, with `/GS`
stack protection) then fails to link with undefined:

| Symbol | Provided by |
|---|---|
| `memcmp` (other vcruntime exports) | `vcruntime.lib` (import of vcruntime140.dll) |
| `__security_cookie`, `__security_check_cookie` | static part of `msvcrt.lib` (`/GS` support) |
| `_tls_index` | static part of `msvcrt.lib` (TLS support, `tlssup.obj`) |

## Attempt and result (reverted)

Linking `vcruntime.lib` and `msvcrt.lib` for every C or C++ reference whose system include directory holds
`vcruntime.h` resolved those symbols but broke executables that linked before: `msvcrt.lib(fltused.obj)` was
selected for `_fltused` ahead of NativeSharp's runtime, which defines it too, giving `duplicate symbol: _fltused`
with both lld-link and NSLink. The change was reverted.

## Options for a general solution

1. Link `vcruntime.lib` for C references as for C++ (resolves the vcruntime exports only; low risk).
2. Link `msvcrt.lib` after NativeSharp's runtime library, so every symbol NativeSharp already defines (`_fltused`
   and any other) resolves to NativeSharp's definition and only the missing support objects (`/GS` cookie, TLS) come
   from `msvcrt.lib`. Requires the archive order to be guaranteed in both linkers, and a check that no other
   `msvcrt.lib` member conflicts.
3. NativeSharp's runtime provides the compiler-support symbols that `/MD` objects reference (`__security_cookie`,
   `__security_check_cookie`, `__report_gsfailure`, `_tls_index` with the TLS directory), as it already provides
   `_fltused` and the ucrtbase support imports. No MSVC static object is linked; more runtime code to own.

In every option the CRT startup does not run (`__security_init_cookie` is not called), so the `/GS` cookie keeps a
fixed value unless NativeSharp initializes it.

Libraries built with the static runtime (`/MT`, `libcmt.lib`) are not covered by any option and cannot be mixed
with `msvcrt.lib`.

## Status

Not implemented: the choice between options 2 and 3 changes the link contract of every executable and is pending a
decision.
