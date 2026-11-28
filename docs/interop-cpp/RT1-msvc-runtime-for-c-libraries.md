# RT1 - MSVC runtime support for static C and C++ libraries built with /MD

## Problem

An executable links with `/nodefaultlib` and NativeSharp's own entry point, so the default libraries named in the
objects of a referenced static library (`/DEFAULTLIB:msvcrt.lib`, `oldnames.lib`) are not linked. The compiler
linked runtime import libraries from the toolset and SDK that a reference's system include directories identify:
`ucrt.lib` for any reference, and `msvcprt.lib` + `vcruntime.lib` only for C++ references.

A static C library compiled against the dynamic MSVC runtime (`/MD`, the default of MSVC and clang-cl, with `/GS`)
then failed to link with undefined:

| Symbol | Provided by |
|---|---|
| `memcmp` (other vcruntime exports) | `vcruntime.lib` (import of vcruntime140.dll) |
| `__security_cookie`, `__security_check_cookie` | static part of `msvcrt.lib` (`/GS` support) |
| `_tls_index` | static part of `msvcrt.lib` (TLS support) |

## Rule

For a library reference whose system include directory is an MSVC toolset include directory (it holds
`vcruntime.h`), the compiler links from that toolset's `lib/x64`:

| Reference | Libraries |
|---|---|
| C++ | `msvcprt.lib`, `vcruntime.lib` (as before) |
| static library (a `Library` with no `Runtime` DLL), C or C++ | `vcruntime.lib`, `msvcrt.lib` |

An import library of a DLL brings no code into the image, so its link is unchanged.

NativeSharp's runtime library is placed before the libraries of the references in the executable link, so a symbol
that both NativeSharp's runtime and `msvcrt.lib` define (`_fltused`) resolves to NativeSharp's definition, and
`msvcrt.lib` only supplies what NativeSharp does not define. Archive members are linked only for referenced symbols,
so the CRT entry points are not linked and the process entry point stays NativeSharp's.

## Limits

- The CRT startup does not run, so `__security_init_cookie` is not called: the `/GS` cookie keeps its default
  value (the checks still run with a fixed cookie). The executable's TLS is set up by the Windows loader from the TLS
  directory that `msvcrt.lib` contributes.
- Libraries built with the static runtime (`/MT`, `libcmt.lib`) are not given `libcmt.lib`.
- Linking every reference with toolset includes against `msvcrt.lib` (not only static libraries) was tried first and
  rejected: with NSLink, `msvcrt.lib(fltused.obj)` was still loaded and duplicated NativeSharp's `_fltused`.

## Changes

- `src/Driver/LibraryReferenceValidation.cpp`: runtime import libraries derived from system include directories.
- `src/Linker/WindowsExecutableBuilder.cpp`: NativeSharp's runtime library before the reference libraries.

## Status

Built (Release). Compiled and linked a set of programs: executables whose static `/MD` C library needed these
symbols now link, and executables that linked before still link. The executables were not run.
