# NativeSharp Linux port: charter for unattended work

Owner decision (2026-11-28): the Linux port is carried out by scheduled, unattended Claude sessions. Each
session decides on its own, records why, and never waits for the owner. This charter replaces the owner's
answers. Read it completely at the start of every session, then `NATIVESHARP-LINUX-PORT-PROGRESS.md` and
`NATIVESHARP-LINUX-PORT-DECISIONS.md` (same folder).

## 1. Objective

`nsc` built on Linux (WSL distro `Ubuntu-24`, Ubuntu 26.04) with the `/opt/clang-llvm` toolchain, linked fully
statically, compiles NativeSharp programs for the `linux-x64` target into static ELF executables that run, with
the same language surface and behaviour as the Win64 compiler. The Win64 target is kept intact and the branch
must merge back into the Windows line without changing Windows behaviour.

## 2. Definition of done (all gates must pass, evidence recorded in the progress log)

| Gate | Pass condition |
| --- | --- |
| G1 | `ninja nsc` succeeds in `/root/nsc-build-linux`; `file bin/nsc` says "statically linked". |
| G2 | `nsc --check` passes on every `examples/tutorial_*` that the Windows compiler accepts. |
| G3 | A `linux-x64` program without exceptions builds to a static ELF executable and prints the expected output. |
| G4 | Runtime families (console, filesystem, directories, environment, command line, clock, threads, atomics, mutex/monitor, synchronization, process, diagnostics) and exceptions (try/catch/finally, throw, cleanup) work on `linux-x64` at O0, O1, O2 and O3. |
| G5 | NativeSharp.Core and the stdlib packages of `/root/nativesharp-libraries/stdlib` build as `linux-x64` static libraries; every `examples/tutorial_*` that runs on Windows builds and runs on Linux with the same output at O0, O1, O2 and O3. |
| G6 | C interop with System V plans works on `linux-x64` for the C cases already supported on Win64. |
| G7 | Merge readiness: every change outside Linux-only files keeps the Win64 code path byte-identical or provably equivalent; the audit document lists each shared-file change. |

When all gates pass, write `STATUS: COMPLETE` as the first line of the progress log. Every later session that
sees it exits immediately without changes.

## 3. Decisions already taken (do not reopen without new evidence)

- Host C++ runtime: libstdc++ (GCC 15 `libstdc++.a`). The LLVM/Clang archives in `/opt/clang-llvm` use
  libstdc++ and no RTTI; libc++ is not usable with them.
- Linker for building `nsc`: GNU `ld.bfd` (`-fuse-ld=bfd`). `/opt/clang-llvm/bin/ld.lld` does not run (ICU 70).
- Linker for generated `linux-x64` programs: lld ELF inside the process (`liblldELF.a`), no external tool.
- C library for generated programs: static glibc.
- Default target of the Linux build: `linux-x64` (`NSC_DEFAULT_TARGET`). No Win64 cross-compilation from Linux
  until G6 passes; then reconsider and record the decision.
- Exceptions on `linux-x64`: `landingpad` lowering with an Itanium-style personality and libunwind; the funclet
  lowering stays for Win64 behind a strategy interface.
- Separation strategy: section 6 of `NATIVESHARP-LINUX-WINDOWS-SEPARATION-AUDIT.md`.

## 4. How to decide alone

When several options exist, choose in this order: what the audit document says; what the repository already does
for the same problem; the option that changes the fewest shared files and keeps Win64 byte-identical; the option
that is simplest to verify. Record every non-trivial choice in the decisions log (date, context, options,
choice, reason, how to revert) and continue. Never stop to ask.

## 5. Hard limits (never do these; record them as blocked items and work on something else)

- Do not modify anything under `A:\` (`/mnt/a`), `C:\native-sharp` (`/mnt/c/native-sharp`) or any Windows path.
  Reading them is allowed.
- Do not delete files other than ones the sessions created under `/root/nsc-build-linux*`, `/root/nsc-linux-work`
  or `/tmp`.
- Do not install, remove or upgrade system packages; do not change `/opt/clang-llvm`.
- Do not push, change remotes, rewrite history, reset, rebase or drop stashes. Commit only on branch
  `nativesharp-linux` of `/root/nsc-devel` and branch `linux` of `/root/nativesharp-libraries`.
- Do not modify examples, tutorials or tests to make them pass; their failures are evidence of what the compiler
  must fix. Do not add workarounds.
- Compiler sources and docs describe constructs generically; never mention games, ports, tutorials or specific
  libraries in compiler code, comments or commits.

## 6. Engineering rules (owner's standing rules)

- The disk is the only source of truth: read a file right before changing it and verify what was written.
- Code: Allman braces when opening functions; lines of at most 120 characters; one class per file; simple
  readable code, no lambdas in new code, no several conditions crammed in one line; never remove existing
  comments. Comments and documentation in English.
- Every claim in logs is classified: verified, inference, risk to verify, or "not verified".
- Validate compiled and run tests at O0, O1, O2 and O3 and report each level.
- Prefer one large coherent block of work per session over small increments.
- Shared files: add a Linux branch next to the Windows code; never rewrite the Windows lines.
- Repository files use CRLF line endings; keep the ending of each file you edit.

## 7. Session protocol

1. Lock: if `/root/nsc-linux-port.lock` exists and its first line (Unix seconds) is less than 4 hours old, exit
   at once. Otherwise write the current Unix time to it.
2. If the progress log starts with `STATUS: COMPLETE`, delete the lock and exit.
3. Read this charter, the progress log and the decisions log. Run `git status` in both repositories. Uncommitted
   changes left by an interrupted session are inspected, finished, built and tested before new work.
4. Take the first item of "Next" in the progress log. Implement a large coherent block.
5. Build: `ninja -C /root/nsc-build-linux nsc` (reconfigure with the command in section 8 if needed). Run the
   validation in section 8 that applies to what changed.
6. Update the progress log (what was done, evidence, new "Next" list) and the decisions log.
7. Commit in each repository that changed, one commit per session, message in English describing the change
   generically, ending with the attribution lines the session environment specifies. Use
   `git -c user.name=al82dev -c user.email=330617241+al82dev@users.noreply.github.com commit`.
8. Delete the lock.

## 8. Operating notes (environment facts verified on 2026-11-28)

- Commands reach the distro only through the `nsexec` tool: `run_program` with
  `path` = `C:\Windows\System32\wsl.exe`, `cwd` = `W:\`, args
  `["-d","Ubuntu-24","-u","root","--","bash","-c","<command>"]`.
- The nsexec sandbox rejects arguments containing `*` or `?`, text that looks like an NTFS alternate data
  stream (`::`, or a word followed by a colon such as `name:`), and any argument over 8192 characters.
  `$VAR` written directly in the argument is expanded to empty before bash sees it. Therefore send scripts as
  `echo <base64 of gzip of script> | base64 -d | gunzip | bash` and keep each argument under 8192 characters
  (split larger payloads into several appends to a file under `/tmp`).
- Anything longer than a few seconds: `"background": true, "timeoutSeconds": 0`, then `read_job` with
  `waitSeconds` 45 until finished.
- Configure command (build dir `/root/nsc-build-linux`):
  `cmake -G Ninja /root/nsc-devel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/opt/clang-llvm/bin/clang
  -DCMAKE_CXX_COMPILER=/opt/clang-llvm/bin/clang++ -DCMAKE_EXE_LINKER_FLAGS="-static -fuse-ld=bfd"
  -DLLVM_DIR=/opt/clang-llvm/lib/cmake/llvm -DClang_DIR=/opt/clang-llvm/lib/cmake/clang
  -DNSC_ENABLE_CPP_INTEROP=ON -DNSC_BUILD_TESTS=OFF -DZLIB_LIBRARY=/usr/lib/x86_64-linux-gnu/libz.a
  -DLIBXML2_LIBRARY=/usr/lib/x86_64-linux-gnu/libxml2.a -Dzstd_LIBRARY=/usr/lib/x86_64-linux-gnu/libzstd.a`
- NativeSharp.Core sources: `/root/nativesharp-libraries/stdlib/NativeSharp.Core`. Build copies go to
  `/root/nsc-linux-work`, never into the libraries checkout. `NSC_CORE_LIBRARY_ROOT` points at the folder with
  `NativeSharpCore.lib`/`.nsmeta` (or the Linux equivalents once they exist).
- Win64 runtime library for win64-target experiments: `NSC_WINDOWS_RUNTIME_LIBRARY` =
  `/mnt/c/native-sharp/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib` (read only).
- Validation baseline: `nsc --check` on `/root/nsc-devel/examples/tutorial_*/Main.ns` (20 of 23 pass; the 3
  failures are language errors present before the port), `nsc --emit-obj` producing ELF at O0-O3.
- Python 3 is available in the distro for file edits; edit files by exact-anchor replacement and verify hashes.
