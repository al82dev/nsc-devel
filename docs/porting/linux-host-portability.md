# Linux host portability of the compiler sources

These changes let the compiler sources build on a non-Windows host. Windows behavior is unchanged: every new
path is in the `#else` branch of `#if defined(_WIN32)`, or is a change that is equivalent on every host.

## Environment variables read with `getenv` outside Windows

| Variable | Where | Use |
|---|---|---|
| `NS_LIB_ROOT` | `src/Compilation/ProjectLoader.cpp`, `src/Driver/CompilerInvocation.cpp` | Absolute fallback root for library references and transitive binary dependencies (`libraryReferenceFallbackRoot`) |
| `NSC_LINKER` | `src/Driver/CompilerInvocation.cpp` | Linker to use |
| `NSC_NO_ERROR_LOG` | `src/nsc/main.cpp` | `1` disables the error log |
| declared comptime variables | `src/Frontend/ComptimeEngine.cpp` | Comptime environment reads, with the same unset and size-limit errors as on Windows |

`src/Linker/ExternalImportValidation.cpp`: hosts other than Windows have no Windows system directories, so only
explicit DLL paths are checked there.

## Portable library use

- Missing standard headers added where the sources relied on transitive includes: `<charconv>`, `<cstring>`,
  `<cstdlib>`.
- `std::to_string` of a `time_since_epoch().count()` casts to `long long`, whose overload exists on every standard
  library (`FileDigestCache.cpp`, `CachePruneIndex.cpp`, `LinkToolIdentity.cpp`).
- `std::ranges::fill` replaced by `std::fill` in `OwnershipSemantics.cpp`.

## Build

`CMakeLists.txt`: when LLVM and Clang are built without RTTI and the compiler is not MSVC, the three sources that
derive from Clang classes (`ClangCancellationFileSystem.cpp`, `ClangDiagnosticConsumer.cpp`,
`ClangSessionAction.cpp`) are compiled with `-fno-rtti`, so they do not reference type information that LLVM does
not provide. This condition can also hold for a Clang (non-MSVC) build on Windows against an LLVM without RTTI.

## Status

The source changes were present in the Windows Release O3 builds made after they appeared, and those builds
succeeded. The `CMakeLists.txt` change has not been built on Windows yet. Not built on Linux from this repository
state here.
