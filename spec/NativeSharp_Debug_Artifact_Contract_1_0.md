# NativeSharp Debug Artifact Contract 1.0

**Status:** normative candidate for NativeSharp 1.0.1 debug support  
**Supported target:** x86_64-pc-windows-msvc  
**Artifact format:** standard CodeView in COFF and Microsoft PDB (MSF)  
**Private sidecar formats:** not supported and not required

This document defines the compiler-facing contract between `nsc`, the PE linker and a native
debugger. The words **must**, **must not**, **shall** and **shall not** are normative.

## NSC-DEBUG-001 — Independent optimization and debug selections

Optimization and debug information are independent selections. `-O0` through `-O3` shall not
implicitly enable or disable debug information.

The compiler shall accept these case-sensitive debug options:

- `-g0`: emit no CodeView debug information and request no program PDB;
- `-gline-tables-only`: emit source files, procedures and line tables;
- `-g`: emit the full supported NativeSharp debug profile.

If more than one debug option is present, the last one wins. The default remains `-g0` for
compatibility. A debug option used by an action that cannot emit an object, library or executable
shall be diagnosed as a command-line error unless that action explicitly documents inspection of
the selected profile.

## NSC-DEBUG-002 — Standard Windows artifacts

For executable output `<name>.exe`, every enabled debug profile shall request `<name>.pdb` from the
selected PE linker. The executable shall contain an RSDS CodeView record whose GUID, age and PDB
name identify that PDB. The default recorded PDB name shall be the file name, not a build-machine
absolute path.

COFF objects shall retain standard CodeView sections needed by the linker. NativeSharp shall not
require a proprietary `.nsdbg` sidecar or a private debugger protocol to recover source mappings.

## NSC-DEBUG-003 — Minimum usable profile

The first conforming debug slice shall provide, for user-authored code:

- one compile unit with producer and language identity;
- normalized source file identity;
- procedure records with separate display and linkage names;
- instruction locations sufficient for file-and-line stepping;
- standard frame and unwind information emitted by the Windows x64 backend.

Compiler-generated prologues, epilogues and helper instructions must not systematically appear as
user source statements. A function may legitimately be a leaf without its own `.pdata` entry; the
required behavior is correct stack walking.

## NSC-DEBUG-004 — Full NativeSharp profile

The full `-g` profile shall extend the minimum profile with lexical scopes, parameters, initial
locals, NativeSharp types, variable locations and location ranges. It shall describe values that
move or disappear under optimization without inventing stale locations.

The profile shall cover ordinary methods, constructors, lambdas, callbacks, C exception-boundary
wrappers, static initializers, destruction helpers, virtual/interface dispatch paths and Windows SEH funclets.
Generated procedures shall remain distinguishable from user-authored procedures.

Internal synthesized procedures that own executable behavior, including static initialization,
destruction entries, typed-catch filters and closure-context destruction, shall be marked
artificial but shall not be classified as thunks. ABI adaptation boundaries, including C exports
and callback exception guards, shall be marked both artificial and thunk. This distinction must
survive in the standard CodeView/PDB procedure record kind.

An interface or virtual method table that already stores the concrete implementation entry point
shall not gain a wrapper merely to manufacture a debug frame. Its user-authored implementation is
a normal procedure, and inline table lookup remains attributed to the source call site. Only a
real ABI receiver/signature adjustment may introduce an artificial thunk between those frames.

Windows SEH pads and cleanup continuations that are physical ranges inside a user procedure shall
remain part of that logical procedure; they shall not be published as invented user procedures.
Synthesized typed-catch filters remain artificial procedures because they own executable behavior.
At O0, any mandatory SEH legalization that promotes locals shall retain a stoppable source anchor
for each nonempty catch body, and catch/finally/throw locations shall not be attributed wholesale
to the enclosing `try` statement.

NativeSharp enums shall be emitted as enumerations with their declared underlying integer type and
enumerators. Structs and unions shall use their authoritative physical size, alignment, field
offsets and accessibility. Class references shall point to a named class type that describes the
object layout, base class and instance fields; interfaces remain opaque references unless an
authoritative object layout exists. Native arrays and strings shall expose their physical
length-prefixed header and inline data tail. Managed delegates shall expose the four-pointer ABI-1
cell (`Code`, `Receiver`, `ContextDestroy`, `CellFree`). Closed generic display names shall use a
stable source-level spelling rather than the internal materialization hash. A captured value shall
be described at its capture-cell value address when that address is materialized; an optimized-away
capture shall not receive an invented location.

## NSC-DEBUG-005 — Source identity and reproducibility

Source records shall use normalized paths and content checksums when the LLVM/CodeView producer
supports them. A documented path-remapping mechanism shall allow reproducible artifacts without
publishing private checkout roots.

NativeSharp source records shall carry a SHA-256 checksum of the exact source bytes loaded by the
compiler. Paths shall be encoded as UTF-8, lexically normalized, use `/` separators, and use an
uppercase Windows drive letter when a drive remains in the published path.

`-fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>` remaps only the path published
in debug information; it shall not change which file is opened. The source prefix is matched on a
path-component boundary using Windows ASCII case-insensitive comparison. Multiple options are allowed:
the longest matching prefix wins, and the last option wins ties. The replacement must be nonempty
and is normalized before the unmatched suffix is appended. With no matching option, the normalized
absolute source path is published.

Given equal source bytes, compiler identity, options, target and remapping inputs, debug artifact
generation shall be deterministic to the extent supported by the selected standard linker. Any
linker-controlled nondeterminism shall be measured and documented rather than hidden by a private
post-processing format.

## NSC-DEBUG-006 — Executable evidence

Debug support is conforming only when automated evidence verifies all applicable layers:

1. LLVM IR contains valid compile-unit, file, subprogram and location metadata;
2. emitted COFF contains CodeView debug sections;
3. `lld-link` produces a PDB and matching RSDS record;
4. `llvm-pdbutil` reports the expected module, procedure, source file and line information;
5. O0 and O2 fixtures preserve executable behavior while retaining their declared debug profile.

Human stepping in Visual Studio or WinDbg is a release acceptance check, not a substitute for the
automated artifact checks.
