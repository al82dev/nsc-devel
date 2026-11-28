# NativeSharp: Linux / Windows separation audit

Branch: `nativesharp-linux` at `/root/nsc-devel` (WSL, Ubuntu 26.04.1 LTS).
Toolchain under audit: `/opt/clang-llvm` (Clang/LLVM 23.1.0).
Date: 2026-11-28.

## 0. Scope, method and classification

Scope: everything in the compiler tree that ties `nsc` to Windows, split along two independent axes:

- **Host**: the operating system `nsc` itself runs on.
- **Target**: the operating system, object format and ABI of what `nsc` generates.

The audit inventories the coupling, measures it, and proposes how to separate it so that a Linux host and a
`linux-x64` target can be added without changing Windows behaviour, and merged back later.

Method:

1. Read-only `grep`/`find` inventory of `src/`, `include/`, `CMakeLists.txt`, `cmake/`, `tests/`, `sdk/`.
2. One out-of-tree probe build on Linux (no file inside the repository was modified):
   `/root/nsc-build-linux-probe2`, target `nsc`, `ninja -k 0`, logs in `/tmp/nscaudit/`.
3. Inspection of the toolchain binaries and static archives in `/opt/clang-llvm` and the system.

Every statement carries one tag:

- **[V] Verified fact**: confirmed by code, files, tools or the probe build.
- **[I] Inference**: conclusion drawn from evidence; the evidence is named.
- **[R] Risk to verify**: plausible problem; a directed test is pending.
- **[NV] Not verified**: "I have not verified this."

No percentages, durations or effort estimates are given; none were measured.

## 1. Executive summary

- [V] The Linux probe build of `nsc` reached 602 build steps; **13 translation units failed with 24 errors**.
  All other compiled translation units of the `nsc` target compiled on Linux with libc++. Linking was not
  reached.
- [V] The 24 errors fall into five mechanical groups (section 3). None of them requires an architectural change.
- [V] The real coupling is on the **target** side: the target is not a parameter that flows through the
  pipeline. `TargetInfo::WindowsX64()`, `TargetDataLayout::WindowsX64()` and
  `LlvmTargetMachine::createWindowsX64()` are called directly from the Parser, Frontend, MIR/HIR, Driver,
  LLVM backend and Language Service (section 4.2).
- [V] Exception handling is lowered with the Windows funclet model (`catchswitch`, `catchpad`, `cleanuppad`,
  personality `__CxxFrameHandler3` / `__C_specific_handler`). Linux needs the `landingpad` model with an
  Itanium-style personality. This is the deepest change inside the LLVM lowering (section 4.6).
- [V] The runtime of generated programs is produced by `WindowsRuntimeBuilder.cpp` (7896 lines) as LLVM IR
  calling 39 distinct Win32 functions, behind a surface of 100 `__nativesharp_*` symbols. That surface is the
  natural OS-neutral contract for a Linux runtime (section 4.8).
- [V] `/opt/clang-llvm/bin/ld.lld` cannot run on Ubuntu 26.04: it needs `libicu*.so.70`, the system ships
  ICU 78. `clang`, `clang++` and `llvm-ar` run. A decision is needed on the linker (section 8).
- [V] Every static archive needed for a fully static `nsc` exists: libc++, libc++abi, libunwind, compiler-rt
  builtins, glibc (`libc.a`, `libm.a`, `librt.a`, `libdl.a`, `libpthread.a`), `libz.a`, `libzstd.a`,
  `libxml2.a`, `liblzma.a`. [NV] A static link has not been attempted.

## 2. Toolchain facts

| Item | Result |
| --- | --- |
| `clang`, `clang++` 23.1.0 | [V] Run. Dynamic deps: libm, libz, libstdc++, libgcc_s, libc. |
| `ld.lld`, `lld` | [V] Do not run: `libicui18n.so.70`, `libicuuc.so.70`, `libicudata.so.70` not found. |
| System `ld.lld` | [V] `/usr/bin/ld.lld -> ld.lld-21` exists (system LLVM 21, not `/opt/clang-llvm`). |
| System `ld.bfd` | [V] Present; used by the probe configure step successfully. |
| LLVM static libraries | [V] 222 `libLLVM*.a`; `liblldELF.a`, `liblldCOFF.a`, `liblldCommon.a` present. |
| libc++ runtime | [V] `lib/x86_64-unknown-linux-gnu/`: `libc++.a`, `libc++abi.a`, `libunwind.a`. |
| compiler-rt | [V] `lib/clang/23/lib/x86_64-unknown-linux-gnu/libclang_rt.builtins.a`, `clang_rt.crtbegin.o`. |
| CMake packages | [V] `lib/cmake/llvm`, `lib/cmake/clang`, `lib/cmake/lld`. |
| `LLVMSupport` link interface | [V] `rt;dl;m;ZLIB::ZLIB;zstd::libzstd_static;LLVMDemangle`. |
| libxml2 users | [V] Only `libLLVMWindowsManifest.a` references `xmlReadMemory`. |
| ICU users in LLVM static libs | [V] None found (`nm -u` scan of all `libLLVM*.a`). |
| Host tools | [V] CMake 4.2.3, Ninja, 16 hardware threads. |

## 3. Probe build: every compile error

Configure command (out of tree):

```text
cmake -G Ninja /root/nsc-devel -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_COMPILER=/opt/clang-llvm/bin/clang -DCMAKE_CXX_COMPILER=/opt/clang-llvm/bin/clang++
  -DCMAKE_CXX_FLAGS=-stdlib=libc++ -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -fuse-ld=bfd"
  -DLLVM_DIR=/opt/clang-llvm/lib/cmake/llvm -DNSC_NSLINK_EXECUTABLE=/bin/true -DNSC_BUILD_TESTS=OFF
```

- [V] Without `-DNSC_NSLINK_EXECUTABLE`, configure stops at `CMakeLists.txt:58` (`FATAL_ERROR`, NSLink
  required). `/bin/true` was passed only to get past that check for the probe.
- [V] Configure then succeeded; it also warned that `NSC_TEST_COMPILER` defaults to
  `C:/native-sharp/bin/nsc.exe` (`CMakeLists.txt:1613`).
- [V] `-DNSC_ENABLE_CPP_INTEROP` was left at its default (`OFF`). [NV] The Clang interop library was not
  built on Linux.

### 3.1 Group A: missing `#include <charconv>` (libc++ does not include it transitively)

| File:line | Error |
| --- | --- |
| `src/Compilation/ProjectLoader.cpp:879` | `no member named 'from_chars' in namespace 'std'` |
| `src/Frontend/Bodies/OwnershipAnalyzer.cpp:1110` | same |
| `src/Frontend/Bodies/PatternAnalyzer.cpp:337` | same |
| `src/Frontend/MemberModel/TypeIdentity.cpp:294` | same |
| `src/LLVM/Lowering/Support/NumericHelpers.cpp:72`, `:88` | same |

Resolution: add `#include <charconv>`. [I] Behaviour-neutral on Windows.

### 3.2 Group B: `std::to_string` on a `file_time_type` tick count

| File:line |
| --- |
| `src/Compilation/FileDigestCache.cpp:29` |
| `src/LLVM/Internal/CachePruneIndex.cpp:126` |
| `src/Linker/Internal/LinkToolIdentity.cpp:33` |

[V] Each calls `std::to_string(x.time_since_epoch().count())` on a `std::filesystem::file_time_type`.
[I] libc++ represents that clock's ticks as `__int128`, for which no `to_string` overload exists, hence the
ambiguity. MSVC STL uses a 64-bit integer.
Resolution: `std::to_string(static_cast<long long>(...count()))`. [R] This changes the text only if a tick count
exceeds 64 bits; verify the cache keys stay identical on Windows.

### 3.3 Group C: `std::ranges::fill` on `std::vector<bool>`

| File:line | Error |
| --- | --- |
| `src/Language/OwnershipSemantics.cpp:16` | `no matching function for call to object of type 'const __fill'` |

[V] `state.initializedFields` is filled with `std::ranges::fill`. [I] It is a `std::vector<bool>` (proxy
reference); libc++ rejects it. Resolution: `std::fill(v.begin(), v.end(), false)`.

### 3.4 Group D: MSVC CRT extensions used outside `_WIN32` guards

| File:line | Symbol |
| --- | --- |
| `src/Compilation/ProjectLoader.cpp:1198` | `_dupenv_s` |
| `src/Driver/CompilerInvocation.cpp:68`, `:414` | `_dupenv_s` |
| `src/Frontend/ComptimeEngine.cpp:343` | `errno_t`, `_dupenv_s` |

[V] Further `_dupenv_s` / `_putenv_s` sites exist that compiled because they are inside `_WIN32` blocks or in
Windows-backend files: `src/Linker/LldLinker.cpp:80`, `src/Linker/WindowsExecutableBuilder.cpp:73`,
`src/Linker/WindowsRuntimeImports.cpp:55`, `src/Linker/WindowsRuntimeLibrary.cpp:68`,
`src/Driver/Internal/ProjectCommandSupport.cpp:87`, `src/Driver/Internal/EnvironmentOverride.cpp:32`, `:41`.

Resolution: one host function `readEnvironmentVariable(name) -> std::optional<std::string>` (section 6.2).

### 3.5 Group E: unguarded Windows headers and APIs

| File:line | Error |
| --- | --- |
| `src/nsc/main.cpp:38` | `DWORD`, `GetEnvironmentVariableA` outside the `_WIN32` block at lines 24-28 |
| `src/Linker/ExternalImportValidation.cpp:14` | `#include <Windows.h>` without guard (PE import validation) |

Resolution: `main.cpp:38` goes through the same host environment function. `ExternalImportValidation.cpp` is
part of the Win64 backend; see section 6.4 for whether the Win64 backend is built on a Linux host.

### 3.6 What the probe did not reach

- [NV] Link of `nsc` (static or dynamic), and every link-time issue.
- [NV] Runtime behaviour of any `#else` / non-Windows path that compiled (section 5.2).
- [NV] Tests (`NSC_BUILD_TESTS=OFF`), Clang interop (`NSC_ENABLE_CPP_INTEROP=OFF`), self-host.

## 4. Target coupling inventory

### 4.1 State of the target abstraction

- [V] `include/nativesharp/Target/TargetInfo.h` defines `TargetInfo` with `architecture()`,
  `operatingSystem()`, `objectFormat()`, `llvmTriple()`, `abiName()`, CPU feature queries and the factories
  `WindowsX64()`, `WindowsX64(preferred, available)`, `WindowsArm64()`.
- [V] `OperatingSystem` has one value: `Windows`. `ObjectFormat` has one value: `COFF`.
  `Architecture` has `X86_64`, `Arm64`.
- [V] `include/nativesharp/Target/TargetDataLayout.h` has `WindowsX64()` and `WindowsArm64()`.
- [V] A process-global CPU level exists: `selectedWindowsX64CpuFeatureLevel()` and `CpuFeatureLevelScope`.
- [V] `src/LLVM/LlvmTargetMachine.cpp:25-39` initialises the X86 and AArch64 LLVM targets.

[I] The abstraction is shaped correctly; it is missing values (`Linux`, `ELF`) and, more importantly, it is not
passed through the pipeline (4.2).

### 4.2 Hard-wired target factories (direct calls instead of a selected target)

[V] Call sites outside `src/Target/`:

| Area | File:line |
| --- | --- |
| Parser | `src/Parser/StatementParser.cpp:627`, `:836`, `:1007`, `:1106` |
| Language | `src/Language/MemoryModel.cpp:54` (`atomicAccessIsLockFreeWindowsX64`), `src/Language/AbiSemantics.cpp:55` (`callingConventionSupportedOnWindowsX64`) |
| Language Service | `src/LanguageService/LanguageService.cpp:1091` |
| Frontend | `src/Frontend/FrontendCompilation.cpp:19`, `src/Frontend/ProjectObjectLayoutModel.cpp:370`, `include/nativesharp/Frontend/ProjectObjectLayoutModel.h:255` |
| MIR | `src/Frontend/MIR/Internal/MirNodeMachine.cpp:464`, `:544`, `:1455`, `src/Frontend/MIR/MirCache.cpp:141` |
| HIR | `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp:302`, `src/Frontend/HIR/HirAggregateLowering.cpp:505`, `:990` |
| Driver | `src/Driver/CompilerSession.cpp:182`, `:187`, `src/Driver/Driver.cpp:190`, `:237`, `:433`, `src/Driver/DriverProjectActions.cpp:575`, `src/Driver/CompilationInputManifestFactory.cpp:11`, `src/Driver/ToolchainContract.cpp:29`, `src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp:772` |
| LLVM | `src/LLVM/Internal/Implementation.cpp:214`, `src/LLVM/CppLifetimeObjectEmitter.cpp:31`, `:44`, `:76`, `src/LLVM/ProjectOopObjectGenerator.cpp:442`, `:1142`, `:1202` |
| Win64 backend | `src/Linker/WindowsExecutableBuilder.cpp:263`, `:850`, `src/Linker/WindowsLibraryBuilder.cpp:152`, `src/Linker/WindowsRuntimeLibrary.cpp:152`, `src/Runtime/WindowsRuntimeBuilder.cpp:6612`, `:6670`, `:6851`, `:6897`, `:6930` |

[V] No `--target` option is parsed by the driver. The only `--target=` strings are arguments passed to an
external Clang in `src/Linker/LldLinker.cpp:1586`, `:1687`.

### 4.3 Triple string literals

[V] Files outside `src/Target/`, `src/Linker/`, `src/Runtime/` containing `windows-msvc` / `pc-windows`
(occurrence count):

| File | Count |
| --- | --- |
| `src/Interop/Cpp/CppSemanticRecord.cpp` | 10 |
| `src/Interop/Cpp/CppProjectSessions.cpp` | 9 |
| `src/Frontend/NameLookup/QualifiedTypeNames.cpp` | 9 |
| `include/nativesharp/Interop/CppAbiPlanQueries.h` | 8 |
| `src/Interop/Cpp/CppSemanticInventory.cpp` | 7 |
| `src/Interop/Cpp/CppSemanticFunction.cpp` | 7 |
| `src/Interop/Internal/CppCRecordMetadataCodec.cpp` | 6 |
| `include/nativesharp/Interop/CppRegCallAbiValidation.h` | 4 |
| `src/LLVM/Lowering/OopLlvmDeclarations.cpp`, `src/LLVM/CppLifetimeObjectEmitter.cpp`, `src/Interop/LibraryMetadataVerification.cpp`, `src/Interop/Cpp/ClangSessionAction.cpp`, `src/Interop/Cpp/ClangSession.cpp`, `src/Frontend/ProjectNameLookup.cpp`, `src/Driver/ProjectCommands.cpp` | 2 each |
| `src/Language/SmartPointerSemantics.cpp`, `src/Language/MemoryModel.cpp`, `src/Language/LanguageSurface.cpp`, `src/Language/CoroutineSemantics.cpp`, `src/LLVM/LlvmCApi.h`, `src/LLVM/Internal/X64InstructionWalker.cpp`, `src/LLVM/Internal/OopLlvmImplementation.h`, `src/LLVM/Internal/Implementation.cpp`, `src/Interop/Internal/LibraryMetadataCodecSupport.h`, `src/Interop/Cpp/CppSemanticFunctionGroups.cpp`, `src/Interop/Cpp/ClangAbiPlan.cpp`, `src/Frontend/ProjectCompilation.cpp`, `src/Frontend/NameLookup/SyntaxBinding.cpp`, `src/Driver/Internal/BuildPreferences.h`, `src/Driver/Driver.cpp`, `src/Driver/DiagnosticBundle.cpp`, `include/nativesharp/LLVM/LlvmModuleMetadata.h`, `include/nativesharp/Interop/CppVariadicAbiValidation.h`, `include/nativesharp/Interop/CppSysVRecordAbiValidation.h`, `include/nativesharp/Interop/CppCRecordAbiReplay.h`, `include/nativesharp/Interop/CppAbiPlan.h` | 1 each |

Totals: [V] 36 files in `src/` + `include/`; [V] 54 files in `tests/`.

[V] Two usage patterns exist:

1. Manifests that write the literal: `src/Language/LanguageSurface.cpp:99`, `MemoryModel.cpp:78`,
   `SmartPointerSemantics.cpp:140`, `CoroutineSemantics.cpp:745` (`target=x86_64-pc-windows-msvc`).
2. Decisions keyed on the literal: e.g. `src/Frontend/NameLookup/QualifiedTypeNames.cpp:692`, `:721`, `:750`,
   `:814`, `:844`, `:894`, `:928`, `:1123` (`selected->targetTriple == "x86_64-pc-windows-msvc"`).

[I] Pattern 2 encodes a *concept* (Microsoft C++ ABI, Win64 C convention), not "Windows". Replacing the literal
by an OS check would be wrong; it must become a concept query (section 6.3).

### 4.4 Direct use of Win64 backend classes outside the backend

[V] Files outside `src/Linker/`, `src/Runtime/` that include or use `WindowsExecutableBuilder`,
`WindowsLibraryBuilder`, `WindowsRuntimeBuilder`, `WindowsRuntimeLibrary`, `WindowsRuntimeImports`,
`LldLinker`, `PeExecutableVerifier`, `CoffArchiveVerifier`, `PdbVerifier`:

| File | Uses |
| --- | --- |
| `src/Driver/Driver.cpp` | includes 7 backend headers; `LldLinker::discover` (`:354`); `WindowsRuntimeBuilder` (`:374`) |
| `src/Driver/Internal/DriverCompilationUnit.h` | includes 8 backend headers (`:36`, `:102-113`) |
| `src/Driver/DriverArtifactGeneration.cpp` | `WindowsExecutableBuilder` (`:742`, `:928`, `:997`, `:1151`), `WindowsLibraryBuilder` (`:1467`, `:1572`, `:1707`, `:1981`, `:2018`, `:2129`, `:2220`), `PeExecutableVerifier` (`:1381`), `configuredWindowsRuntimeLibrary` (5 sites) |
| `src/Driver/ThinLtoLibraryLinking.cpp` | `configuredWindowsRuntimeLibrary` (`:54`, `:370`), `CoffArchiveVerifier` (`:181`), `LldLinker` (`:281`) |
| `src/Driver/PrecompiledModuleConfiguration.cpp` | `LldLinker` (`:226`), runtime library (`:151`, `:236`), `PeExecutableVerifier` (`:324`) |
| `src/Driver/LibraryReferenceValidation.cpp` | `PeExecutableVerifier` (`:125`, `:387`, `:837`), `CoffArchiveVerifier` (`:133`, `:237`, `:287`, `:461`) |
| `src/Driver/DriverLibraryCommands.cpp` | `PeExecutableVerifier` (`:346`), `PdbVerifier` (`:390`), `CoffArchiveVerifier` (`:424`, `:483`) |
| `src/Driver/RuntimeClosurePackaging.cpp` | `PeExecutableVerifier` (`:75`, `:172`, `:259`, `:426`, `:489`) |
| `src/Driver/CompilerActionDispatcher.cpp` | `writeWindowsRuntimeLibrary` (`:69`), `verifyWindowsRuntimeLibrary` (`:71`), `PeExecutableVerifier` (`:374`) |
| `src/Driver/CompilerInvocation.cpp` / `.h` | `LldLinker::validateAddressSanitizerRuntime` (`:344`); header include (`.h:12`) |
| `include/nativesharp/Driver/Driver.h` | includes `LldLinker.h` (`:8`) |
| `include/nativesharp/LLVM/LlvmModule.h` | `friend class runtime::WindowsRuntimeBuilder` (`:42`) |

Correction to the earlier chat estimate: [V] the real count is 13 files (not 11); all but `LlvmModule.h` are in
the Driver.

### 4.5 Calling conventions and C/C++ ABI

- [V] 48 files outside Linker/Runtime/Target mention `Win64`; the largest: `src/Interop/Cpp/CppProjectSessions.cpp`
  (76), `include/nativesharp/Interop/CppRegCallAbiValidation.h` (65), `src/LLVM/Lowering/OopLlvmDeclarations.cpp`
  (48), `src/Interop/Internal/CppCRecordMetadataCodec.cpp` (47), `src/LLVM/Lowering/OopLlvmClosuresDelegatesEvents.cpp`
  (34), `src/Frontend/HIR/HirBuildCoordinator.cpp` (26), `src/LLVM/Lowering/OopLlvmCalls.cpp` (23).
- [V] Example: `OopLlvmDeclarations.cpp:79-109` selects return lowering through
  `interop::abi_plan::isRegCallWin64*` predicates; `:249` documents "Win64 aggregate ABI (ground truth: clang for
  x86_64-pc-windows-msvc)"; `:267`, `:316` handle variadic C imports "of the Win64 C convention, without a SysV
  plan".
- [V] SysV-named code already exists: `include/nativesharp/Interop/CppSysVRecordAbiValidation.h`,
  `src/LLVM/Internal/CppSysVRecordAbi.h`; 30 files mention `SysV`/`Itanium`/`linux-gnu`.
- [NV] Whether the SysV code paths are reachable or tested today.
- [V] `dllimport`/`dllexport` in 8 files (largest: `src/Interop/Cpp/ClangSession.cpp` 16,
  `src/LLVM/Lowering/OopLlvmCalls.cpp` 10); `src/Interop/CHeaderGenerator.cpp:899-901` emits
  `__declspec(dllexport/dllimport)` into generated C headers.
- [V] `__chkstk` referenced in `src/Frontend/MemberModel/BuildAndDeclaration.cpp`.

### 4.6 Exception handling model

[V] The lowering emits the Windows funclet EH model:

- `src/LLVM/Lowering/OopLlvmCleanup.cpp:309` (`eh.cleanuppad`), `:1890-1903` (personality
  `__CxxFrameHandler3`), `:2186-2189` (`__C_specific_handler` behaviour).
- `src/LLVM/Lowering/OopLlvmControlFlow.cpp:129` (`eh.catchswitch`), `:141` (`eh.catchpad`), `:363` (O0 SEH
  legalization), `:468` ("`__C_specific_handler` -- the Windows x64 personality this compiler targets").
- Also: `OopLlvmClosuresDelegatesEvents.cpp:236`, `OopLlvmStructuredExits.cpp`, `OopLlvmThunksAndWrappers.cpp`,
  `src/LLVM/Internal/OopLlvmImplementation.h`, `src/LLVM/LlvmCApi.h`, `src/LLVM/LlvmOptimizer.cpp`.
- Runtime side: `__nativesharp_eh_filter`, `__nativesharp_eh_init`, `__nativesharp_exception_*`,
  `buildPanicEhFilter` (`src/Runtime/WindowsRuntimeBuilder.cpp:182`), `RaiseException` import.

[I] On ELF/Linux, LLVM requires `landingpad`-based EH with a personality routine (Itanium ABI, unwinding through
libunwind). Funclet pads are not lowered for that target. This needs an EH strategy seam in the lowering with two
implementations; it is the highest-risk item of the Linux target.

### 4.7 Object format and debug info

[V] COFF/CodeView-specific code outside the Win64 backend (32 files mention COFF), main ones:
`src/LLVM/Internal/CoffObjectFile.cpp` (27), `src/LLVM/Internal/ThinLtoBackend.cpp` (17),
`src/LLVM/Internal/CodeViewProcedureRanges.cpp` (15), `src/LLVM/Internal/CoffObjectFile.h` (12),
`src/LLVM/CoffObjectVerifier.cpp` (12), `src/LLVM/ProjectOopObjectGenerator.cpp` (10), `src/Driver/Driver.cpp`
(7), `src/Driver/DriverArtifactGeneration.cpp` (6).

[V] File-extension literals (`".exe"`, `".dll"`, `".lib"`, `".obj"`, `".pdb"`) in 10 files, e.g.
`src/Driver/ProjectCommands.cpp` (7), `src/Driver/ArtifactPublisher.cpp` (5), `src/Compilation/ProjectLoader.cpp`
(5), `src/LLVM/ProjectOopObjectGenerator.cpp` (4), `src/LLVM/CompilationArtifactCache.cpp` (3).

### 4.8 Runtime of generated programs

- [V] `src/Runtime/WindowsRuntimeBuilder.cpp`: 7896 lines; builds LLVM IR modules (`build`, `buildSupport`,
  `buildEntry`, sanitizer adapters, `buildDynamicLibraryAnchor`).
- [V] It is organised in families: foundations, time/environment, filesystem, command line, threading,
  condition variables, synchronization, process/system, diagnostics, exception state
  (`declareRuntime*` at `:6939-7328`, `buildRuntime*Family` at `:7481-7774`).
- [V] 100 distinct `__nativesharp_*` symbol names; 39 distinct Win32 functions referenced (heap, TLS, threads,
  SRW locks, events, files, directories, process, clock).
- [V] `sdk/win64/nativesharp-runtime.nsimports`: import closure from `KERNEL32.dll`, `KERNELBASE.dll`,
  `SHELL32.dll`, `ucrtbase.dll`.
- [V] `src/Runtime/RuntimeAbiContract.cpp` (290 lines) parses and canonicalises the runtime contract.

[I] The `__nativesharp_*` surface plus `RuntimeAbiContract` is already the OS-neutral boundary. A Linux runtime
builder must implement the same symbols over libc/syscalls; the families map one-to-one.

### 4.9 Linking and packaging

- [V] `src/Linker/` (6638 lines): PE/COFF/PDB writers and verifiers, `LldLinker` (external `lld-link`/Clang
  process), Windows runtime library and imports.
- [V] `CMakeLists.txt:25` defaults `NSC_NSLINK_EXECUTABLE` to `C:/native-sharp/bin/nslink.exe`; `:58-62` makes it
  mandatory.
- [I] For `linux-x64`, ELF linking can be done in-process with `liblldELF.a` (present) or with an external
  `ld.lld`; decision in section 8.

### 4.10 Libraries and SDK

- [V] `lib/` holds 7 `.nsproj` projects, including `NativeSharp.Win32.Compiler` (`LinkerAdapters.ns`,
  `CompilerMappedFile.ns`, `CompilerServices.ns`); `lib/.source.txt` says "Synced from configured NativeSharp
  installation".
- [V] `sdk/` has only `win64/`.
- [NV] Which Core library sources call Win32 directly; Core sources are not in this tree.

## 5. Host coupling inventory

### 5.1 Files with `_WIN32` / `windows.h` / `_MSC_VER` (33 files)

`win` = guard/include lines, `else` = `#else`/`#elif` lines; APIs = Win32/CRT names found.

| File | win | else | APIs |
| --- | --- | --- | --- |
| `include/nativesharp/Basic/OutputFailureKind.h` | 1 | 0 | |
| `src/Basic/ArtifactTransaction.cpp` | 9 | 7 | `CreateFileW` `MoveFileExW` `DeleteFileW` `FlushFileBuffers` `GetFileAttributesW` |
| `src/Compilation/CompilationBundleCache.cpp` | 1 | 0 | |
| `src/Compilation/PrecompiledProjectPreparation.cpp` | 2 | 0 | |
| `src/Driver/CompilationTelemetry.cpp` | 2 | 0 | |
| `src/Driver/Driver.cpp` | 2 | 0 | |
| `src/Driver/DriverArtifactGeneration.cpp` | 6 | 6 | `LoadLibraryA` `GetProcAddress` |
| `src/Driver/DriverPathSupport.cpp` | 1 | 1 | `MultiByteToWideChar` |
| `src/Driver/Internal/DriverCompilationUnit.h` | 2 | 0 | |
| `src/Driver/Internal/EnvironmentOverride.cpp` | 2 | 2 | `_putenv_s` |
| `src/Driver/Internal/ProjectCommandSupport.cpp` | 3 | 2 | `GetModuleFileNameW` |
| `src/Driver/PackageLifecycle.cpp` | 2 | 2 | |
| `src/Driver/PrecompiledSurface/PathKeyCaseFolding.cpp` | 2 | 0 | `LCMapStringEx` |
| `src/Driver/ProjectCommands.cpp` | 6 | 4 | `CreateProcessW` `WaitForSingleObject` `MultiByteToWideChar` |
| `src/Frontend/Cache/CacheEntryStore.cpp` | 4 | 3 | `GetCurrentProcessId` `MoveFileExW` |
| `src/Interop/CHeaderGenerator.cpp` | 2 | 5 | |
| `src/Interop/Cpp/CppBinaryDependencies.cpp` | 2 | 1 | `CompareStringOrdinal` |
| `src/LLVM/CppLifetimeObjectEmitter.cpp` | 2 | 1 | `CompareStringOrdinal` |
| `src/LLVM/Internal/ArtifactCacheFilesystemPath.cpp` | 2 | 1 | |
| `src/LLVM/Internal/ArtifactCacheFilesystemPath.h` | 1 | 0 | |
| `src/LLVM/Internal/CacheEntryLock.cpp` | 2 | 1 | `CreateFileW` |
| `src/LLVM/Internal/CacheEntryLock.h` | 2 | 1 | |
| `src/LLVM/Internal/ColdLlvmTrace.cpp` | 2 | 1 | |
| `src/LLVM/Internal/StandardErrorCapture.cpp` | 1 | 0 | (POSIX block `#if !defined(_WIN32)`, `:3-56`) |
| `src/LLVM/Internal/StandardErrorCapture.h` | 1 | 0 | `_dup2` |
| `src/LLVM/Internal/ThinLtoBackend.cpp` | 2 | 3 | |
| `src/LLVM/LlvmOptimizer.cpp` | 6 | 2 | `CreateProcess` |
| `src/LLVM/LlvmProcessTrace.cpp` | 4 | 1 | `CreateProcessW` `GetModuleFileNameW` `GetTempPathW` `GetStdHandle` |
| `src/Linker/LldLinker.cpp` | 18 | 10 | `CreateProcessW` `GetEnvironmentVariableW` `GetModuleFileNameW` |
| `src/Linker/WindowsExecutableBuilder.cpp` | 1 | 1 | `GetEnvironmentVariable` `SetEnvironmentVariable` |
| `src/Linker/WindowsRuntimeImports.cpp` | 3 | 2 | (names are import data for generated programs) |
| `src/Linker/WindowsRuntimeLibrary.cpp` | 3 | 1 | `GetModuleFileNameW` |
| `src/nsc/main.cpp` | 5 | 2 | `GetEnvironmentVariableA` `WideCharToMultiByte` |

### 5.2 Classification

- [V] **Guarded with an alternative branch** (`else` > 0): 20 files. They compiled on Linux, except where a
  Group D/E error sat outside the guard.
- [V] **Additive Windows-only blocks** (`else` = 0, not a POSIX-only block): `OutputFailureKind.h`,
  `CompilationBundleCache.cpp:289-301`, `PrecompiledProjectPreparation.cpp:1145-1193`,
  `CompilationTelemetry.cpp:182-188`, `Driver.cpp:41-45`, `DriverCompilationUnit.h:42-46`,
  `PathKeyCaseFolding.cpp:19-27`, `ArtifactCacheFilesystemPath.h:7-9`.
  [R] On Linux these compile to "no Windows behaviour"; each must be checked for a silent behaviour difference
  (example: `PathKeyCaseFolding.cpp` uses `LCMapStringEx` only on Windows).
- [V] **Unguarded**: `src/nsc/main.cpp:38`, `src/Linker/ExternalImportValidation.cpp:14` (section 3.5).
- [NV] Whether each `#else` branch is functionally complete or a "not supported" stub.

### 5.3 Other host assumptions

- [V] 14 files in `src/` contain string literals with escaped backslashes. [NV] How many are paths.
- [V] Hard-coded Windows paths in CMake: `CMakeLists.txt:25` (`C:/native-sharp/bin/nslink.exe`), `:766`
  (`A:/vc2026-runtime`), `:798` (Visual Studio installer), `:1613` (`C:/native-sharp/bin/nsc.exe`),
  `:3246-3290` (`NSC_LINKER=nslink.exe`, `NSC_WINDOWS_RUNTIME_LIBRARY`). 107 files in `cmake/` contain a drive
  path or `.exe`.
- [V] 332 Python files under `tests/` reference `.exe`, `nsc.exe` or `C:` paths.

## 6. Resolution: how to separate

### 6.1 Principles

1. Windows behaviour does not change. Every step on the Windows side is a refactor proven by the existing
   Windows build and tests.
2. Linux is additive: new files, new enum values, new CMake files. Existing Windows files stay where they are
   (moving files multiplies merge conflicts).
3. Two axes, never mixed: host code never asks for the target; target code never asks for the host.
4. Decisions are keyed on concepts (`usesMicrosoftCxxAbi`, `objectFormat() == ELF`), not on OS names or
   triple text.
5. One class per file; no new `#ifdef _WIN32` outside the host layer.

### 6.2 Host layer

New interface headers in `include/nativesharp/Host/`, one function family per file, two implementations:
`src/Host/Windows/` (code moved from the 33 files, unchanged) and `src/Host/Posix/` (new). CMake compiles one
directory according to `WIN32`.

| Service | Replaces |
| --- | --- |
| `EnvironmentVariables` (read/set/unset) | `_dupenv_s`, `_putenv_s`, `GetEnvironmentVariable*`, `SetEnvironmentVariable*` |
| `ExecutablePath` | `GetModuleFileNameW` |
| `ProcessLauncher` (spawn, wait, capture) | `CreateProcess*`, `WaitForSingleObject`, pipes |
| `AtomicFileReplace` / durable write | `MoveFileExW`, `FlushFileBuffers`, `DeleteFileW` in `ArtifactTransaction`, `CacheEntryStore` |
| `FileLock` | `CreateFileW` share-mode locking in `CacheEntryLock` |
| `TextCaseFolding` / ordinal compare | `LCMapStringEx`, `CompareStringOrdinal` |
| `DynamicLibrary` | `LoadLibraryA`, `GetProcAddress` |
| `StandardStreamRedirect` | `_dup2` / POSIX block of `StandardErrorCapture` |
| `TextEncoding` (UTF-8/UTF-16 at OS boundary) | `MultiByteToWideChar`, `WideCharToMultiByte` |
| `ProcessIdentity`, `TempDirectory` | `GetCurrentProcessId`, `GetTempPathW` |

Groups A-C of section 3 are plain portability fixes and need no layer.

### 6.3 Target threading

1. Add `OperatingSystem::Linux`, `ObjectFormat::ELF`, `TargetInfo::LinuxX64()`, `TargetDataLayout::LinuxX64()`.
2. Add concept queries to `TargetInfo`: `usesMicrosoftCxxAbi()`, `usesWin64CConvention()`,
   `usesSysVCConvention()`, `usesFuncletExceptionHandling()`, `executableSuffix()`, `objectSuffix()`,
   `staticLibrarySuffix()`, `sharedLibrarySuffix()`, `debugInfoFormat()`.
3. One selected `TargetInfo` per compilation, owned by `CompilerInvocation`/`CompilerSession` and passed down.
   The 40+ direct factory calls of 4.2 read it instead. `LlvmTargetMachine::create(const TargetInfo&, level)`
   replaces `createWindowsX64`.
4. Driver option `--target=win64|linux-x64`, default = host. The selected value goes into every manifest that
   today writes the literal (4.3 pattern 1).
5. Pattern-2 literals (4.3) become concept queries from step 2.
6. Rename `selectedWindowsX64CpuFeatureLevel` to a target-neutral name; the CPU level applies to any x86-64
   target.

### 6.4 Backend interfaces

Abstract classes (one per file) in a new `include/nativesharp/Backend/`:
`ExecutableBuilder`, `LibraryBuilder`, `RuntimeBuilder`, `LinkTool`, `ImageVerifier`, `ArchiveVerifier`,
`DebugInfoVerifier`, plus `BackendFactory` that returns the Win64 or LinuxX64 implementation from `TargetInfo`.

- Win64: the existing classes implement the interfaces; their logic does not change.
- LinuxX64: new classes in `src/Backend/LinuxX64/` (ELF writer via LLVM, ELF linking, runtime builder).
- The 13 Driver files of 4.4 call the interfaces. `LlvmModule.h:42` gets a neutral friend or accessor.

Open question (owner): whether a Linux-hosted `nsc` must also build the Win64 backend (cross-compilation). It
needs a COFF linker on Linux (`liblldCOFF.a` exists [V]; `nslink.exe` does not run on Linux [I]) and
`ExternalImportValidation.cpp` without `<Windows.h>`.

### 6.5 Runtime for linux-x64

New `LinuxRuntimeBuilder` emitting the same 100 `__nativesharp_*` symbols, family by family (the
families listed in section 4.8), over libc (static glibc) or raw syscalls.
`RuntimeAbiContract` is shared. `sdk/linux-x64/` holds the
equivalent of the `.nsimports` closure.

### 6.6 Exception handling

An EH strategy seam in the lowering (`OopLlvmCleanup`, `OopLlvmControlFlow`, `OopLlvmStructuredExits`,
`OopLlvmClosuresDelegatesEvents`, `OopLlvmThunksAndWrappers`):

- `FuncletExceptionLowering`: the current code, moved behind the interface unchanged.
- `LandingPadExceptionLowering`: new, Itanium personality, unwinding through libunwind.

The runtime EH pieces (`__nativesharp_eh_*`, exception TLS) get a Linux implementation. [R] The O0 SEH
legalization pass (`OopLlvmControlFlow.cpp:363`) must stay Win64-only.

### 6.7 C/C++ interop ABI

Interop plans stay per-target: Win64/regcall predicates for `win64`, SysV plans for `linux-x64`. The existing
SysV files (4.5) are the starting point. `CHeaderGenerator` emits `__attribute__((visibility("default")))`
instead of `__declspec` for ELF targets.

### 6.8 CMake

- `NSC_TARGETS` (default `win64` on Windows hosts, `linux-x64` on Linux hosts).
- NSLink is required only when `win64` is enabled and the host is Windows.
- Split sources into `cmake/HostWindows.cmake`, `cmake/HostPosix.cmake`, `cmake/TargetWin64.cmake`,
  `cmake/TargetLinuxX64.cmake`.
- Every `C:/`, `A:/`, Visual Studio and DIA SDK setting stays under `if(WIN32)`.
- `NSC_TEST_COMPILER` default derived from the build tree, not `C:/native-sharp`.
- Linux static profile in `cmake/HostPosix.cmake` (section 7).

### 6.9 Tests

- Label tests by target (`win64`, `linux-x64`, `any`) so `ctest -L` selects them.
- The 54 C++ test files with triple literals and the 332 Python files with `.exe`/`C:` paths get the target
  and paths from configuration.
- Per the owner's rules, every runnable test is validated at O0, O1, O2 and O3.

## 7. Static `nsc` on Linux

[I] Link line shape (to be verified):

```text
clang++ -static -stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=<linker>
  <objects> <LLVM static libs> -lz -lzstd -lxml2 -llzma -lrt -ldl -lm -lpthread
```

- [V] All listed archives exist (section 2).
- [R] `LLVMSupport` requires `dl`; static glibc links `dlopen` with a warning and it only works if the matching
  glibc shared objects are present at run time. `nsc` must not rely on it.
- [R] Static glibc warns for NSS functions (`getpwnam`, `getpwuid_r`, `getaddrinfo`). LLVM uses `getpwuid_r` for
  the home directory. Check that `nsc` never reaches it, or accept the warning.
- [R] `libxml2.a` may need further static dependencies (ICU, lzma). Only `LLVMWindowsManifest` needs libxml2; a
  Linux-only `nsc` without the Win64 backend may not need it.
- Acceptance: `file nsc` reports "statically linked"; `ldd nsc` reports "not a dynamic executable".

## 8. Decisions for the owner

1. **Linker for building `nsc`**: `/opt/clang-llvm/bin/ld.lld` does not run (ICU 70 missing). Options:
   (a) system `ld.lld-21`; (b) GNU `ld.bfd`; (c) repair `/opt/clang-llvm` (provide ICU 70 or replace its `lld`).
   Compiler and libraries stay from `/opt/clang-llvm` in all three.
2. **ELF linking of generated programs**: in-process `liblldELF.a` or external `ld.lld`.
3. **Cross target on Linux host**: whether `win64` is built into the Linux-hosted `nsc` (6.4).
4. **libc for the linux-x64 runtime of generated programs**: static glibc, or raw syscalls without libc.
5. **Clang interop on Linux** (`NSC_ENABLE_CPP_INTEROP`): needs `clang` static libs (present [V]); scope and
   timing.

## 9. Work split and merge plan

### 9.1 Behaviour-neutral work (can be done on the Windows side, proven by the Windows build)

- Groups A-C of section 3 (portability fixes).
- Host layer 6.2 with the Windows implementation only.
- Target threading 6.3 steps 2, 3, 5, 6 with `WindowsX64` as the only value.
- Backend interfaces 6.4 with the Win64 implementation only.
- EH seam 6.6 with `FuncletExceptionLowering` only.
- CMake gating 6.8.

### 9.2 Linux-only work (additive, on `nativesharp-linux`)

- `src/Host/Posix/`, `TargetInfo::LinuxX64`, `TargetDataLayout::LinuxX64`, `src/Backend/LinuxX64/`,
  `LinuxRuntimeBuilder`, `LandingPadExceptionLowering`, SysV interop completion, `cmake/HostPosix.cmake`,
  `cmake/TargetLinuxX64.cmake`, `sdk/linux-x64/`, Linux test labels.

### 9.3 Merge-conflict hotspots

[I] Files that both lines are likely to edit: `src/Driver/DriverArtifactGeneration.cpp`, `src/Driver/Driver.cpp`,
`src/Driver/Internal/DriverCompilationUnit.h`, `src/Interop/Cpp/*`, `src/LLVM/Lowering/OopLlvm*.cpp`,
`src/Frontend/NameLookup/QualifiedTypeNames.cpp`, `CMakeLists.txt`.
If the Linux branch must touch them before 9.1 lands, the change is limited to an added branch; the existing
Windows lines stay byte-identical.

### 9.4 Gates

| Gate | Pass condition |
| --- | --- |
| G1 | `nsc` builds and links statically on Linux with the `/opt/clang-llvm` compiler and libraries. |
| G2 | `nsc --check` and frontend tests pass on Linux. |
| G3 | A `linux-x64` program without exceptions builds, links to ELF and runs. |
| G4 | Runtime families (console, filesystem, threads, sync, process, clock) and EH pass on `linux-x64`, O0-O3. |
| G5 | C interop with SysV plans passes on `linux-x64`. |
| G6 | Windows build of the merged tree: same tests passing as before, same artifacts. |

## 10. Not audited

- [NV] `nativesharp-interop/` (10208 files), `selfhost/` (350 files), `tools/`, `book/`, `examples/`.
- [NV] Core library sources (not in this tree).
- [NV] Behaviour of Windows code paths; nothing was built or run on Windows for this audit.

## 11. Evidence left on disk

- `/root/nsc-build-linux-probe/`: first configure attempt (failed on `ld.lld`).
- `/root/nsc-build-linux-probe2/`: probe build tree.
- `/tmp/nscaudit/`: `configure.log`, `build.log`, `host_files.txt`, `triple_sites.txt`.
  [V] `/tmp` in WSL is not persistent across distro restarts.

Nothing inside `/root/nsc-devel` was modified except this document.
