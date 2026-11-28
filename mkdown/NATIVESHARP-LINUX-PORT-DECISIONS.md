# NativeSharp Linux port: decisions log

Newest first. Each entry: date, context, options, choice, reason, how to revert.

## 2026-11-28 — libstdc++ instead of libc++ for the host

- Context: the LLVM/Clang static archives in `/opt/clang-llvm` reference `std::__cxx11` (libstdc++) symbols and
  were built with `LLVM_ENABLE_RTTI OFF`.
- Options: libc++ (link fails), libstdc++.
- Choice: libstdc++ from GCC 15, linked statically.
- Revert: only possible with an LLVM build against libc++.

## 2026-11-28 — `ld.bfd` to link `nsc`

- Context: `/opt/clang-llvm/bin/ld.lld` needs `libicu*.so.70`, which Ubuntu 26.04 does not ship.
- Options: system `ld.lld-21`, `ld.bfd`, repairing `/opt/clang-llvm`.
- Choice: `ld.bfd` (system packages and `/opt/clang-llvm` stay untouched).
- Revert: change `-fuse-ld` in the configure command.

## 2026-11-28 — per-file `-fno-rtti` for Clang interop sources

- Context: `ClangCancellationFileSystem.cpp`, `ClangDiagnosticConsumer.cpp` and `ClangSessionAction.cpp` derive
  from LLVM/Clang classes whose type information does not exist in RTTI-less archives; `ClangSession.cpp` uses
  `dynamic_cast` and keeps RTTI.
- Choice: `set_source_files_properties(... COMPILE_OPTIONS -fno-rtti)` only when `NOT MSVC AND NOT
  LLVM_ENABLE_RTTI`.
- Revert: remove the block in `CMakeLists.txt`.

## 2026-11-28 — default target selected at build time

- Context: the compiler had no target parameter; 40 call sites created Windows x64 directly.
- Choice: `NSC_DEFAULT_TARGET` (win64 | linux-x64, default by host) and `DefaultX64()` factories. A command-line
  `--target` is deferred until cross-compilation is reconsidered (charter section 3).
- Revert: configure with `-DNSC_DEFAULT_TARGET=win64`.
