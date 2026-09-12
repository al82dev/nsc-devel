# NativeSharp Compiler Contract 1.0.1

**Status:** current development contract for NativeSharp 1.0.1
**Supported compiler line:** 1.0.1-dev
**Supported LLVM line:** 23.x
**Supported target:** x86_64-pc-windows-msvc

This document defines failure behavior and release-hardening obligations of `nsc`. Language
semantics remain in `../language/NativeSharp_Language_Specification_1_0_1.md`.

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

NativeSharp 1.0.1 supports LLVM 23.x. A configured LLVM package outside that major shall be rejected
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

## NSC-LINK-001 — Windows executable subsystem

An executable shall use `IMAGE_SUBSYSTEM_WINDOWS_CUI` by default. The explicit driver option
`/subsystem:windows` shall instead emit `IMAGE_SUBSYSTEM_WINDOWS_GUI`; `/subsystem:console` shall
select the default explicitly. Option names and values are case-insensitive, may appear anywhere
in an executable-emission command and shall be rejected for actions that do not emit an
executable. Unsupported or duplicate subsystem options are command-line errors.

The selected subsystem shall reach every supported PE linker without changing the NativeSharp
entry-point ABI. The PE verifier shall accept both supported values and reject other subsystem
values.

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

## NSC-PKG-004 — Deterministic installation inventory

The supported installation workflow shall derive an ordered expected-file list from the public
payload selected by the current source tree, never by accepting every file already present in the
destination prefix. The list shall use normalized forward-slash relative paths and the versioned
`NSFILES 1` text format.

After copying the payload, the newly installed compiler shall write
`NativeSharpInstallation.manifest` in the versioned `NSINSTALL 1` text format. Every entry shall
contain the lowercase SHA-256 digest, decimal byte size and normalized relative path of one expected
file. Entries shall be sorted by path. The manifest shall exclude itself and include the expected
file list that defined its scope.

## NSC-PKG-005 — Complete installation integrity verification

`nsc --verify-installation-manifest <prefix> <manifest>` shall reject malformed headers or entries,
non-canonical paths, duplicate or reordered entries, missing files, size differences, SHA-256
differences and unexpected regular files inside the complete trees owned by this package:
`share/nativesharp/spec`, `share/nativesharp/toolchain` and each library subdirectory selected by
the expected-file list. Under the shared `lib` directory, only those selected subdirectories are
complete package-owned trees;
unrelated archives may coexist beside them. The exact `bin/nsc.exe` path is authenticated, while
unrelated tool executables may coexist in the shared `bin` directory. Diagnostics shall name the
first offending relative path.

The CMake install workflow shall generate and immediately verify the inventory through the newly
installed compiler. A package whose bytes or managed file set differ from the expected current
payload shall not be reported as a successful installation.

## NSC-PKG-006 — Independent-build reproducibility

`nsc --compare-installation-manifests <left> <right>` shall compare two canonical installation
manifests by normalized path, size and SHA-256. The command shall emit a versioned `NSREPRO 1`
report and fail when a deterministic path is missing or its contents differ.

Only paths whose format intentionally embeds the per-build identity may be classified as volatile.
For the current package those paths are `bin/nsc.exe` and, when distributed, `bin/nsc.pdb`.
Volatility shall be reported by path; it must not silently exempt Core, specifications, contracts,
metadata or library products from byte-for-byte comparison.

The independent-build gate shall additionally build both compiler executables with one reserved,
fixed build identity and require their complete SHA-256 values to match. The override is a gate-only
normalization mechanism; ordinary builds shall continue to receive distinct identities.

## NSC-PKG-007 — Authenticated update and retirement

An update or retirement plan shall use the versioned `NSUPDATE 1` format and bind itself to the
SHA-256 of the previous `NSINSTALL 1` manifest. An update plan shall also bind itself to the SHA-256
of the next manifest. Update plans shall classify the union of old and new paths as `keep`,
`replace`, `add` or `remove`; retirement plans shall contain only removal of manifest-owned paths.

Applying the removal phase shall reject a changed previous or next manifest, an action inconsistent
with those manifests, malformed or reordered paths, unowned removal targets, locally modified
owned files and symbolic-link traversal. All removals shall pass preflight before the first file is
deleted. Files belonging to another tool in a shared `bin` or `lib` directory shall never be
inferred as NativeSharp property and shall survive both update and retirement.

A full retirement shall also remove the bound installed manifest, which cannot contain its own
digest, and prune empty package subdirectories. The installation prefix and the shared `bin`, `lib`
and `share` roots shall survive.

## NSC-COMPAT-001 — Installed compatibility policy

Every distributable build shall generate and install `NativeSharpCompatibility.policy` in the
canonical `NSCOMPAT 1` format. The policy shall independently publish compiler and language
releases, NSMETA read/write versions, native and runtime ABI versions, package format versions and
the deprecation rule. The build id shall not appear in this compatibility artifact.

`nsc --print-compatibility-policy`, `--write-compatibility-policy` and
`--verify-compatibility-policy` shall share one field authority. The installed-layout verifier and
cryptographic inventory shall require and authenticate the same policy bytes.

## NSC-COMPAT-002 — Version requirement decisions

`nsc --check-compiler-compatibility <major.minor.patch>` shall accept only canonical semantic
versions. Before compiler 1.0, compatibility requires the same major and minor with an installed
patch greater than or equal to the requested patch. Starting with 1.0, a compiler may satisfy an
older requirement only inside the same major line and when its minor/patch is not older.

A supported behavior may be removed only after a diagnostic deprecation period of at least one
minor release. A safety correction or miscompilation fix may reject formerly accepted input, but
the compatibility notes shall identify that exception explicitly.
