# NativeSharp Compiler Contract 1.0

**Status:** normative candidate for the NativeSharp 1.0 compiler  
**Supported compiler line:** 0.9.x  
**Supported LLVM line:** 23.x  
**Supported target:** x86_64-pc-windows-msvc

This document defines failure behavior and release-hardening obligations of `nsc`. Language
semantics remain in `NativeSharp_Language_Specification_1_0.md`.

The words **must**, **must not**, **shall** and **shall not** are normative.

## NSC-FAIL-001 — Unhandled compiler failures

An unexpected C++ exception that reaches the executable boundary shall produce exit code 70 and a
diagnostic beginning with `nsc: internal error:`. It must not escape through `main` as an unreported
termination. Normal errors in user input remain compilation or command-line errors, not internal
errors.

## NSC-DIAG-001 — Diagnostic budget

A compilation shall retain at most 100 errors by default. Once that limit is reached, later
diagnostics shall be suppressed and the renderer shall report how many were omitted. The budget
prevents malformed input from producing unbounded cascades without converting the retained errors
into success.

## NSC-DIAG-002 — Source location

A source diagnostic shall contain path, one-based line, one-based column, severity and cause. When
source text is available, the renderer shall show the affected line and a stable marker.

## NSC-IR-001 — LLVM verification

Every LLVM module shall pass the LLVM verifier before it can be optimized or emitted as a release
artifact. Verification failure shall be returned as an internal compiler error; the invalid module
must not be passed to the linker.

## NSC-LLVM-001 — Supported LLVM major

NativeSharp 0.9 supports LLVM 23.x. A configured LLVM package outside that major shall be rejected
by CMake. A monolithic library discovered without package metadata shall be checked through
`LLVMGetVersion` before lowering, verification, optimization or emission continues.

`nsc --print-llvm-version` shall print the loaded version and the supported major.

## NSC-OPT-001 — Optimization equivalence

O0 and O2 shall preserve the same observable language behavior. Every corrected optimization-only
miscompilation must keep a differential fixture that executes both configurations.

## NSC-EXIT-001 — Exit-code classes

Successful commands return 0, compilation failures return 1, invalid command lines return 2 and
internal compiler failures return 70. A diagnostic class must not silently change between releases
without a compatibility note.

## NSC-NEG-001 — Negative evidence

The release suite shall include malformed lexical, syntactic, semantic, project and command-line
inputs. A negative case passes only when it is rejected through the expected exit class and useful
diagnostic; a crash, hang or invalid LLVM module is a failure.

## NSC-PKG-001 — Deterministic toolchain contract

Every distributable build of `nsc` shall generate and install a canonical
`NativeSharpToolchain.contract` from the identities compiled into that executable. The artifact
shall use the versioned `NSTOOLCHAIN 1` text format, LF line endings, a fixed field order and one
final newline. It shall identify the compatible compiler and language lines, compiler stage,
target, supported LLVM line, NSMETA format, every public binary ABI, managed delegate cell size,
supported linkers, certified optimization configurations and artifact kinds.

The per-build identifier and build timestamp shall not appear in this contract. They identify one
compiler binary but do not change compatibility; including them would make two otherwise
equivalent package contracts differ.

## NSC-PKG-002 — Toolchain contract verification

`nsc --print-toolchain-contract` shall emit the canonical contract without explanatory text.
`nsc --write-toolchain-contract <path>` shall write those exact bytes and create missing parent
directories. `nsc --verify-toolchain-contract <path>` shall reject unsupported headers, malformed
lines, unknown, duplicate, missing, reordered or incompatible fields with a diagnostic that names
the cause. Contract input and compatibility failures return the compilation-error class, never an
internal compiler failure.

## NSC-PKG-003 — Installed layout verification

The supported Windows installation shall contain the compiler executable, its canonical toolchain
contract, the manifest of every distributed Core project and the normative language and compiler
specifications.
`nsc --verify-installation <prefix>` shall validate that complete minimum layout and the contents
of the installed toolchain contract. It shall name the first missing or incompatible component and
return the compilation-error class.

The CMake install workflow shall run this command through the newly installed executable after all
package files have been copied. An incomplete installation must not be reported as successful.
