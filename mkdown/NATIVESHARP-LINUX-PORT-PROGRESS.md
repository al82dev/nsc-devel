# NativeSharp Linux port: progress log

Newest entry first. Each entry: date, session summary, evidence (verified / inference / not verified), and the
updated "Next" list. The "Next" list below is the single queue of work.

## Next

1. Linux runtime: a `LinuxRuntimeBuilder` (one class per file, next to `WindowsRuntimeBuilder`) that emits the
   same `__nativesharp_*` surface (see `RuntimeAbiContract` and section 4.8 of the audit) over static glibc.
   Start with what a program without exceptions needs: entry point (`main` calling the NativeSharp entry), alloc,
   free, console write, trap, exit; then the remaining families.
2. ELF static library builder for `linux-x64` (archive of ELF objects plus `.nsmeta`), so NativeSharp.Core and the
   stdlib packages build for Linux; select it from the target in the driver.
3. ELF executable builder with in-process lld (`liblldELF.a`), static glibc, crt objects; select it from the
   target in the driver (`--emit-exe`, `--emit-project-oop-exe`, `nsc build`).
4. G3: first static `linux-x64` executable for a program without exceptions; record output at O0-O3.
5. Exception handling strategy seam and `landingpad` lowering (audit 6.6).
6. Remaining runtime families; G4.
7. Core and stdlib on Linux; tutorials end to end; G5.
8. System V interop; G6.
9. Merge-readiness review; G7.

## 2026-11-28 — baseline before unattended work

- [V] Branch `nativesharp-linux` contains `interop-sdk-tandas` up to `e4f7ff45d` (merge `66eab8f1f`).
- [V] Portability fixes (13 files), `<cstring>` includes (2 files), per-file `-fno-rtti` for three Clang
  interop sources (non-MSVC only), NSLink gating, `NSC_DEFAULT_TARGET`, Linux target layer, ELF object
  verification: see section 12 of `NATIVESHARP-LINUX-WINDOWS-SEPARATION-AUDIT.md`.
- [V] `bin/nsc` statically linked; `nsc --print-target` = linux / ELF / System V AMD64 ABI /
  x86_64-unknown-linux-gnu; `--emit-obj` gives ELF x86-64 relocatable objects at O0-O3.
- [V] `nsc --check`: 20 of 23 tutorials pass (3 language errors present before the port).
- [NV] Windows build of these shared-file changes.
