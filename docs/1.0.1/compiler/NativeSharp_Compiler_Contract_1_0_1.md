# NativeSharp Compiler Contract 1.0.1

**Status:** current development contract for NativeSharp 1.0.1
**Supported compiler line:** 1.0.1-dev
**Supported LLVM line:** 23.x
**Supported target:** x86_64-pc-windows-msvc

This document defines failure behavior and release-hardening obligations of `nsc`. Language
semantics remain in `../language/NativeSharp_Language_Specification_1_0_1.md`.

The words **must**, **must not**, **shall** and **shall not** are normative.

## NSC-TARGET-002 — Consumer-qualified secondary object target

The backend target layer shall expose Windows ARM64 as `aarch64-pc-windows-msvc`, with the Windows
LLP64 data model, `armv8-a` baseline, architecture-specific feature and register catalogs, and an
LLVM TargetMachine capable of emitting physical ARM64 COFF objects. Certification shall inspect
the emitted COFF machine field (`IMAGE_FILE_MACHINE_ARM64`, `0xaa64`); changing only a label or
triple is not sufficient.

This secondary object-emission capability does not silently widen the current end-to-end product
target stated above. Executable runtime, linker, ABI, debug, and execution matrices remain x64
until each is independently implemented and certified for ARM64.

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

## NSC-DIAG-003 — Deterministic parallel diagnostic reduction

Compiler phases that collect diagnostics in worker-local engines shall reduce those shards by
canonical diagnostic content, never by worker completion order. Valid source locations sort before
locationless diagnostics, followed by file identity, source offset, range end, severity, diagnostic
identifier and message. Diagnostics indistinguishable under that complete key may appear in either
internal order because their rendered content is identical.

The destination engine shall apply the compilation error budget once to the canonical sequence.
Suppression already performed inside a bounded worker-local engine must be added to suppression
performed by the destination; it must not disappear during reduction. Reversing shard arrival order
must preserve the retained diagnostics, counts, error-limit state and suppressed-diagnostic count.

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

Ordinary compilation shall execute directly in the requested compiler process. It shall not create
the LLVM trace-supervisor child process or enable LLVM's verbose per-pass log. Every LLVM module
shall still be verified before and after its required pipeline, and the targeted O0 SEH legalization
passes remain mandatory. `--llvm-pass-trace` on O1, O2, or O3 enables the child-process trace and
per-pass verification explicitly so an optimizer investigation can identify its last completed
LLVM pass without imposing that diagnostic cost on normal builds.

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

## NSC-INPUT-001 — Bounded hostile-input readers

Readers shall validate sizes, counts, offsets and nesting before allocating or indexing. NativeSharp
1.0.1 limits one source to 64 MiB, one project file to 4 MiB, project XML nesting to 128 elements,
one PDB to 1 GiB, a COFF first linker member to 256 MiB and its symbol count to 16,777,216. NSMETA
retains its 128 MiB payload limit. Crossing a limit is an input/compilation failure naming the
format and limit, never an allocation attempt, assertion, hang or internal compiler failure.

File readers must also reject truncation or growth observed between sizing and the complete read;
they shall not silently compile a prefix of a concurrently changed input.

## NSC-GRAPH-001 — Deterministic compilation graph

Source files, project references, library references, metadata-backed synthetic modules, peer
metadata visibility and merged dependency edges shall use canonical case-folded path/module order.
Filesystem enumeration, XML declaration order and metadata discovery order must not change project
ordinals, module visibility, emitted symbol order or diagnostics. Duplicate edges are removed only
after their canonical identity is established. The root source project remains the final graph node.

## NSC-EXEC-001 — Resource budget, cancellation and timing

`--timeout-ms=N` accepts 1 through 86,400,000 and requests cooperative cancellation. Project
compilation observes the deadline between syntax, generics, names, members, inheritance, interfaces,
constraints, bodies, dispatch, destruction, construction, layout, HIR and MIR. Cancellation reports
the next phase by name and returns a compilation failure. It does not interrupt LLVM or a linker
inside a non-transactional call; the next safe boundary observes it.

`--time` reports every project phase that was entered as
`timing phase=<stable-name> us=N`, in deterministic execution order, and finally appends
`timing total_ms=N` on every exit path. Frontend phases include graph loading, source reading,
source and precompiled syntax, generics, names, members, inheritance, interfaces, constraints, bodies,
dispatch, destruction, construction, layout, HIR and MIR. Artifact-producing commands also name
their LLVM, object, library, header and metadata work. Durations use a monotonic clock; phase names
and ordering are stable diagnostic interfaces, while measured values are observational and do not
participate in artifact identity.

Neither timing nor cancellation changes language semantics, optimization or artifact identity. The
diagnostic limit and hostile-input ceilings remain independent resource controls.

O2 has an advisory cumulative LLVM optimization budget of 120,000 milliseconds per compiler
invocation. `--o2-budget-ms=N` may set it from 1 through 86,400,000 milliseconds. `--time` emits
`optimization-budget level=O2 budget_ms=<N> elapsed_us=<N> invocations=<N> status=<within|exceeded>`
when O2 work occurred. Exceeding the budget emits a warning but shall not abort the opaque LLVM
pipeline, change the exit code, or alter the generated artifact.

## NSC-JOBS-001 — Compilation job policy

The compiler shall detect host hardware concurrency, treating an unavailable zero report as one.
Automatic mode reserves one core and uses the remaining capacity, while always retaining at least
one progress job. `-j N` and `-jN` accept 1 through 1,024 and cap requested compiler jobs.
`--reserve-cores=N` accepts 0 through 1,023 and caps both automatic and explicit requests against
available host capacity. Duplicate or malformed controls are command-line errors.

`--print-job-policy` reports hardware threads, requested jobs, reserved cores, effective jobs and
the current scheduler kind. Once project-reference discovery uses the worker pool, the scheduler
shall identify itself as `project-references`; this label does not claim that later compilation
phases are parallel.

## NSC-CACHE-001 — Verified content-addressed object cache

`--artifact-cache=<absolute-directory>` enables object-unit reuse for linked project OOP products.
The option is explicit and invocation-scoped; an absent option performs no cache reads or writes.
Relative paths, duplicate options and use with unsupported actions are command-line errors.

Every object key shall include the exact compiler build identity, loaded LLVM version, target triple,
data layout, optimization level, debug-information mode and complete optimized LLVM text. Cache
paths are derived only from the lowercase SHA-256 key. A stored object has a versioned sidecar that
binds its key, byte size and object SHA-256.

A hit must match the sidecar digest and size, be copied into invocation-owned temporary storage and
pass the ordinary AMD64 COFF verifier before emission is skipped. Missing, incomplete, malformed,
corrupt or non-COFF entries are misses and are rebuilt. A verified miss may be stored for later use.
Parallel units keep independent hit/miss slots, and product output reports their totals.

## NSC-CACHE-002 — Precise incremental invalidation

When the verified object cache is enabled, project OOP code partitioning shall derive each
function's bucket exclusively from its physical function identity and the resolved code-unit count.
Function body size, estimated lowering cost, traversal timing and worker completion order shall not
change that assignment. Empty buckets may be omitted, but surviving bucket ordinals and stable keys
shall not be renumbered.

A body-only edit that preserves public and coordinator state shall invalidate only the code bucket
containing that function. The coordinator and unrelated code buckets shall remain eligible for
verified cache hits. Changes to signatures, global coordination state, target policy or the resolved
code-unit count may invalidate the affected coordinator or partition identities. When the artifact
cache is disabled, the ordinary balanced partition policy remains in effect.

## NSC-WORKERS-001 — Internal compiler worker pool

The internal worker pool shall own a fixed number of joinable threads resolved from the active job
policy. Tasks receive a stable zero-based worker index and a monotonic submission ordinal. Worker
threads shall inherit the resolved job policy. Destruction shall finish queued work and join every
thread; no detached compiler work may outlive the pool.

Task exceptions shall not terminate the process. `wait()` shall first wait for all submitted work,
then rethrow the failure with the lowest submission ordinal and leave the pool reusable. An empty
task is rejected before queueing. This pool provides execution capacity only; dependency order and
deterministic graph scheduling are a separate contract.

## NSC-SCHED-001 — Deterministic dependency scheduling

Compilation tasks shall receive monotonic ordinals and non-empty stable keys. Dependencies may only
refer to earlier ordinals, rejecting self, forward and duplicate edges before execution. The
scheduler shall run topological waves: ready nodes are submitted by stable key and then ordinal,
and no dependent wave starts before every prerequisite in the previous waves completes. A task
failure stops later waves and produces a compilation diagnostic instead of terminating a worker.

## NSC-GRAPH-002 — Parallel project-reference discovery

Independent `ProjectReference` descriptors shall be loaded concurrently through the bounded
compiler worker pool using the effective `-j` policy. Discovery proceeds in stable path-sorted
frontiers. Worker completion order shall not affect duplicate-module diagnostics, cycle diagnostics,
project ordinals, root placement or graph dumps. A one-worker and a many-worker load of the same
unchanged project tree shall produce the same logical graph.

The pool and job policy are linked from the compilation layer so graph loading can use the shared
capacity without introducing a Frontend-to-Driver dependency cycle. Existing Driver-facing headers
remain the compatibility surface for these internal facilities.

## NSC-SYNTAX-001 — Parallel deterministic source syntax

Source file reads, lexing and parsing shall execute as bounded per-file tasks using the effective
compilation job policy. File buffers and syntax units shall be published only by the coordinator in
canonical project/source order, preserving `FileId`, project ordinal and source ordinal assignments.
All buffers shall be installed before concurrent lexer work starts, and all lexer tasks shall finish
before parser-known-type collection and parser tasks begin.

Each task shall collect diagnostics privately. The coordinator shall append those diagnostics in
stable syntax-unit order and preserve each task's internal diagnostic order. A one-worker and a
many-worker compilation of unchanged input shall produce equivalent tokens, syntax validity,
diagnostics and syntax-unit dumps. The dedicated `syntax-read`, `syntax-lex` and `syntax-parse`
timings measure their respective complete parallel stages.

Detailed timing preserves dependency order: the driver loads `precompiled-metadata`, the frontend
builds the `project-graph`, physical `syntax-read` completes, `precompiled-syntax` units are
materialized, and only then do `syntax-lex` and `syntax-parse` run. The report shall include every
reached phase exactly in execution order before `timing total_ms`.

## NSC-NAMES-001 — Parallel per-file declaration and binding indexes

After syntax is complete, each source unit shall independently index namespace declarations and
named-type parts into a private result. The coordinator shall merge those results by syntax-unit
ordinal before partial-type aggregation, assigning global type ordinals only after the merge.
Namespace-tree mutation and duplicate/partial diagnostics remain coordinator operations.

Once the complete declaration index exists, each unit shall independently resolve its type syntax
and expression type receivers against immutable global declaration tables. Per-unit bindings and
diagnostics shall be reduced by unit ordinal; pointer-to-binding maps shall be rebased to the final
global vector offsets. Serial and parallel builds shall expose identical namespace trees, named-type
ordinals, bindings, expression bindings and diagnostics.

## NSC-HIR-001 — Parallel deterministic callable HIR

Eligible written, imported and precompiled callables shall be lowered independently through the
bounded worker pool. Each task shall own its `ProjectOopHirModel` execution state, including cleanup
stacks, exception-handler stacks, lambda context, temporary counters and error range. Shared name,
member, inheritance, interface, body, construction and layout models are immutable task inputs.

The coordinator shall consume callable results by member ordinal and publish the same function order
as serial lowering. Per-task owned-temporary local/cleanup ordinals and HIR exception-region ordinals
shall be rebased into deterministic global ranges before publication. The first failing member in
logical order supplies the error and source range. Lambda callables, field initializers and implicit
constructors remain a coordinator pass because they depend on the completed regular-function table.

## NSC-MIR-001 — Parallel deterministic callable MIR

Every HIR callable shall be transformed to MIR independently through the bounded worker pool when
more than one compilation job is available. Each task shall own its MIR-lowering context and error
buffer. Completed semantic, layout, dispatch, interface, destruction and HIR models are immutable
task inputs; workers shall not mutate the destination function table or member index.

Results shall occupy slots assigned from the canonical HIR function order. Only the coordinator may
publish them into the MIR model and rebuild `functionByMember`, so worker completion order cannot
affect function order or member resolution. The first failure in logical HIR order supplies the
diagnostic. Imported callables remain bodyless records, and field initializers, static constants,
layouts and dispatch/destruction plans remain coordinated passes. Serial and parallel construction
of unchanged input shall produce byte-for-byte equivalent textual MIR dumps.

## NSC-CODEGEN-001 — Deterministic module code partitioning

Before parallel LLVM generation, every defined MIR callable shall be assigned to exactly one code
unit. Imported C and native callables are declarations and shall not own a code unit. The requested
unit count is clamped to the number of definitions, empty units are forbidden and unit identities
shall be stable ordinal keys.

Partitioning shall estimate work from the complete MIR node tree. Candidates are ordered by
descending cost, then physical function identity and canonical MIR ordinal. Each candidate is
assigned to the currently least-loaded unit, breaking ties by unit ordinal. Functions inside each
unit are restored to canonical MIR order. The mapping, costs and textual plan must therefore be
identical for equivalent serial and parallel MIR models and independent of worker completion order.

## NSC-CODEGEN-002 — Parallel LLVM and COFF generation

When more than one compilation job and more than one code unit are available, project product
generation shall lower one coordinator module plus every planned code unit through the bounded
worker pool. The coordinator module owns mutable statics, type initialization, descriptors,
vtables, interface maps, RTTI-visible global data, destruction entries, runtime-bound imports,
callback boundary thunks and C ABI wrappers. Function modules own only their assigned callable
definitions and reference coordinator data through external declarations.

Every independently lowered module must pass LLVM verification. O0 legalization and any requested
optimization are applied to each module before object emission; in this contract revision the
optimization loop remains ordered. COFF emission and validation execute as parallel per-module
tasks. Errors are selected by module ordinal, and object paths are returned in coordinator-first,
code-unit order. EXE, static-library and DLL builders shall accept that ordered object set, append
exactly one runtime object and preserve existing import, export, PDB and verification behavior.

With one effective job, the generator shall retain the compatible single-module/single-object path.
The standalone `--emit-project-oop-obj` action also remains a single-object compatibility surface;
multi-object generation applies to linked project products whose output format can contain or link
multiple COFF inputs.

Optimized full-debug products are a deliberate exception to module partitioning until the compiler
implements authoritative cross-unit inline-summary import. They shall use the single-module path so
LLVM observes caller and callee together and can preserve truthful CodeView inline-site scopes.
O0 full-debug generation and optimized `-g0` or `-gline-tables-only` generation retain the
partitioned path.

## NSC-CODEGEN-003 — Parallel per-unit optimization

After target configuration, every partitioned LLVM module shall be optimized by an independent
`LlvmOptimizer`, observation record and target machine. O1, O2 and O3 module optimization tasks
execute through the same bounded job policy as lowering and emission when the selected debug
profile permits partitioning. Optimized full-debug generation is monolithic under NSC-CODEGEN-002
so inline-site fidelity is not exchanged for throughput. O0 legalization remains an ordered pass
because its mandatory Win64 EH repair is small and avoiding another worker barrier is the
lower-overhead behavior for debug builds.

No optimizer, observation object or mutable LLVM context may be shared by concurrent tasks.
Optimization completion order shall not determine diagnostics: the coordinator inspects success
and error slots by module ordinal. A successful optimized bundle must retain all cross-unit calls,
coordinator references and explicitly exported symbols when linked.

## NSC-CODEGEN-004 — Synthesized symbol ownership and ODR identity

Every synthesized physical symbol shall have an ownership rule derived from its actual linkage
boundary. A helper whose calls and definition stay inside one LLVM module shall use internal
linkage. A helper referenced by peer code units shall use a nominally stable physical name rather
than a project-local ordinal. Independent products must therefore be linkable even when their local
MIR ordinals and generated-helper sets overlap.

Type initializer entry points shall derive their physical identity from the declaring nominal type.
Their guard and owner storage remain private to the coordinator. In a monolithic module the entry
point may remain private; in a partitioned product it is externally declared by peer units and
defined once by the coordinator.

Compile-time constants reconstructed from certified NSMETA values are equivalent definitions.
Producer and consumer copies shall use symbol-specific selectable COMDAT definitions so a DLL
consumer can materialize the value locally and a static-library consumer can merge it with the
producer copy. Mutable imported static storage remains producer-owned and is accessed through its
published accessor.

## NSC-META-001 — Deterministic NSMETA fragment merging

Metadata records collected by independent workers shall be combined by canonical record identity,
never by worker completion order. All fragments must describe the same format, target, producer,
module and frozen ABI header. An incompatible fragment is rejected before any output is published.

Types, members, generic templates, closed specializations, external nominal dependencies, dispatch
tables and C ABI exports have explicit stable ordering keys. Set-like nested collections, including
implemented interfaces, generic dependencies, dispatch interfaces and external type keys, are
sorted and deduplicated. Ordered ABI collections such as parameters, generic arguments and virtual
slots retain their declared order. Exact duplicate records collapse; two different records with
the same canonical identity are an error naming that identity.

Every `LibraryMetadataCodec::writeModel` operation shall pass through the same merger even when the
producer supplies one fragment. Equivalent fragment sets must therefore produce byte-for-byte
identical NSMETA payloads, hashes and ABI fingerprints independent of their arrival order.

Backward-compatibility round trips shall inspect decoded records by canonical identity, not by the
producer model's insertion position, because serialization deliberately canonicalizes order. A
failed or incomplete decode must be reported without dereferencing absent records; malformed legacy
metadata may fail, but the compatibility harness itself must never crash.

## NSC-META-002 — Public member surface and private implementation state

NSMETA member records shall contain only public and protected members declared by publicly reachable
root-project types. Private and internal methods, properties, events and static fields are producer
implementation details and shall not be serialized merely because their declaring type is public.
A type referenced exclusively by such an omitted member shall not be promoted into the public type
closure.

This rule does not erase physical instance layout. Private instance fields that contribute to the
size or offsets of a published type remain represented by its layout records, together with every
type identity required to interpret that layout. Filtering API members and preserving ABI layout are
separate operations.

## NSC-BOUNDARY-001 — Versioned MIR and runtime boundaries

The textual OOP MIR dump shall identify its schema as `major.minor`. A consumer must not infer MIR
semantics from the compiler release alone: an incompatible major requires rejection, while an
additive minor requires explicit support. Node ownership, cleanup, exceptional control flow and
ABI annotations belong to that schema.

The native runtime ABI and the complete runtime symbol set are independently versioned. Changing a
signature, calling convention, layout or behavior requires a runtime ABI change; adding, removing
or renaming a required runtime symbol requires a runtime-symbol-set change. The canonical symbol
inventory is `RuntimeAbiContract.h`. Symbol-set 2 records kind and exact opaque-pointer LLVM type
for all 97 NativeSharp-prefixed exports. Every build shall verify the generated runtime module in
both directions: each contracted symbol is materialized with its declared type and no uncontracted
NativeSharp-prefixed definition leaks from the image. The canonical `NSRUNTIMEABI 1.0` manifest is
installed as `share/nativesharp/runtime/NativeSharpRuntime.abi` and carries target, pointer model,
calling convention, symbol-set version and the ordered typed inventory.

The Windows x64 implementation of that contract is delivered as the independently versioned COFF
archive `lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib`. The archive owns the reusable
runtime support symbols but never the program-specific `__nativesharp_start` entry point. Each
executable receives a separate entry object that calls its selected NativeSharp `Main` and consumes
runtime initialization from the archive. User static libraries shall not embed private runtime
copies; executables and DLLs resolve support from the installed archive. The compiler verifies the
archive before every use. `NSC_WINDOWS_RUNTIME_LIBRARY` may select another verified archive for a
controlled toolchain or test environment. A native C or C++ host that links a NativeSharp static
library directly shall also pass the independent runtime archive before the Win32 import libraries;
the user archive deliberately carries no private runtime copy.

## NSC-PKG-001 — Deterministic toolchain contract

Every distributable build of `nsc` shall generate and install a canonical
`NativeSharpToolchain.contract` from the identities compiled into that executable. The artifact
shall use the versioned `NSTOOLCHAIN 2` text format, LF line endings, a fixed field order and one
final newline. It shall identify the compatible compiler and language lines, compiler stage,
target, supported LLVM line, NSMETA format, every public binary ABI, managed delegate cell size,
the runtime symbol set, MIR schema, supported linkers, certified optimization configurations and
artifact kinds.

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
of the installed toolchain contract, compatibility policy and 2.0 migration baseline. It shall
name the first missing or incompatible component and return the compilation-error class.

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

## NSC-COMPAT-003 — Verifiable 1.x to 2.0 migration baseline

Every distributable build shall generate and install `NativeSharp2Migration.baseline` in canonical
`NSMIGRATION 1` format. It shall freeze the current compiler/language source line, NSMETA, native
and runtime ABI, runtime symbol set, MIR schema, mangling, object, delegate and exception ABI, and
the package policy formats relevant to a future major migration. It shall not claim that 2.0 is
implemented or promoted.

The baseline shall also require a breaking-change ledger, migration notes and an N−1/N
producer-consumer matrix. `--print-migration-baseline`, `--write-migration-baseline` and
`--verify-migration-baseline` shall share one ordered field authority. Missing, reordered,
additional or incompatible fields are compilation failures. Any future 2.0 proposal must classify
each difference from this artifact as preserved, migrated, or intentionally broken before release.

## NSC-INLINE-001 — Authoritative inlining policy

The frontend shall represent every callable with exactly one `InliningPolicy`: `Automatic`,
`Preferred`, `Required`, or `Forbidden`. All later semantic, metadata, optimization, and diagnostic
phases shall consume that vocabulary rather than infer policy independently. Invalid serialized
values fail closed.

`Automatic` permits the target-aware cost model to decide. `Preferred` increases profitability but
never overrides legality. `Required` requires every statically resolved call to expand and shall
produce a source diagnostic when recursion, missing bodies, unresolved dispatch, or another safety
constraint makes that impossible. `Forbidden` must prevent expansion at every optimization level.

Attributes implementing these policies are optimization controls, not ABI declarations. Required
out-of-line definitions, public/export identities, ownership and EH cleanups, dynamic dispatch and
truthful inline debug scopes remain governed by their existing contracts.

## NSC-INLINE-002 — Target-aware automatic inlining

O0 shall disable automatic call expansion. O1, O2, and O3 shall use the target-aware cost model in
their canonical LLVM default pipeline with conservative, balanced, and aggressive intent,
respectively. `LlvmOptimizer` shall obtain both the pipeline and automatic-inlining intent from one
level mapping. It shall not maintain a second undocumented threshold table.

For identical input, target, LLVM version, and optimization level, scheduling or code-partition
completion order shall not affect the decision. Focused evidence shall prove that O0 retains an
ordinary cheap call while every enabled level can expand that call and preserve its result.

## NSC-INLINE-003 — Preferred source modifier

The `inline` keyword shall bind to `InliningPolicy::Preferred` on a concrete ordinary method and
remain explicit in member, HIR, and MIR dumps. LLVM lowering shall attach `inlinehint` to the
definition. The modifier is invalid where no ordinary method body exists, and duplicate uses are a
diagnostic. The hint shall not remove a required out-of-line symbol or override LLVM legality.

## NSC-INLINE-004 — Required source modifier

The `forceinline` keyword shall bind to `InliningPolicy::Required` on a concrete ordinary method and
remain explicit through member, HIR, and MIR models. LLVM lowering shall attach `alwaysinline`, and
the O0 pipeline shall run only the mandatory inliner needed to honor that attribute; automatic O0
inlining remains disabled.

The frontend shall reject placements without an ordinary body, duplicate or contradictory
policies, and recursive cycles whose every participating callable is required-inline. Such cycles
cannot satisfy every static call-site obligation and must not be silently emitted with residual
calls. Public, exported, or address-taken functions retain any independently required out-of-line
identity.

## NSC-INLINE-005 — Forbidden source modifier

The `noinline` keyword shall bind to `InliningPolicy::Forbidden` on a concrete ordinary method and
remain explicit through member, HIR, and MIR models. LLVM lowering shall attach `noinline`, and every
optimization level shall preserve an out-of-line call boundary for that method.

Duplicate uses, abstract declarations, placements without an ordinary body, and combinations with
`inline` or `forceinline` are diagnostics. The modifier controls optimization only; linkage,
visibility, physical identity, calling convention, exports, ownership, and exception behavior remain
unchanged.

## NSC-INLINE-006 — Generic specialization policy

Generic method specialization shall clone the source inlining modifier before member binding.
Explicit and locally inferred specializations shall therefore produce the same
`InliningPolicy`. Every closed physical function shall carry that policy through HIR, MIR, and LLVM;
specialized spelling or cache reuse shall never reset it to `Automatic`.

At O0, focused evidence shall distinguish all three explicit outcomes in one generic call graph:
`Preferred` remains optional, `Required` expands, and `Forbidden` retains its call boundary.

## NSC-INLINE-007 — Value receivers and proven interface targets

Concrete struct methods shall carry inlining policy through their by-address receiver ABI without
boxing or changing layout. Interface calls may become direct calls only after semantic proof of one
exact dynamic class and one implementation slot. A direct interface conversion of a freshly
constructed class is such a proof; the frontend shall resolve it through `ProjectInterfaceModel`
and lower the selected implementation as an ordinary direct call.

Interface parameters, fields, locals, and arbitrary expressions remain dynamically dispatched
unless a separate exact-type proof exists. Merely marking one implementation `forceinline` shall
never suppress a valid interface-map lookup for a polymorphic receiver.

## NSC-INLINE-008 — Cross-library policy and reusable bodies

NSMETA shall preserve the four-valued inlining policy of every published callable. For an ordinary
non-generic `inline` or `forceinline` method, NSMETA 1.13 shall additionally preserve the complete
authored declaration and body. A `LibraryReference` consumer shall analyze and lower that body in
the producer's nominal module identity instead of treating it as an opaque native import. Static,
class-instance, and struct-instance methods use the same rule.

Producer and consumer copies of a transported body shall use the same physical name and
symbol-scoped `weak_odr` COMDAT definition. This permits a static archive or import library to
coexist with a consumer-materialized copy without duplicate strong definitions. `noinline` carries
no body: its metadata policy shall attach `noinline` to the imported declaration and preserve the
producer call boundary. Generic method bodies remain governed by their reusable template closure;
specialization shall not create a second transport representation.

Readers shall reject a transported body attached to a non-method, C import, or policy other than
preferred/required. Writers targeting NSMETA 1.12 or older shall remove the policy bits and body
payload rather than producing a down-level file containing semantics that version never defined.

## NSC-INLINE-009 — Required-inline debug scopes and code-unit ownership

LLVM can expand a call and emit a truthful CodeView inline scope only while caller and callee are
available in the same module. Until code-unit summary import can carry both the required body and
its debug scope, any product containing an `InliningPolicy::Required` callable shall remain a whole
LLVM module at every optimization and debug level. This is a correctness constraint: partitioning
must not leave a residual call that violates `forceinline` or erase its PDB inline site.

With full debug information at O0, a required-inline call shall produce a standard `S_INLINESITE`
record nested under the caller, retain authored body lines and locals, and retain the source call
location. Preferred calls that remain unexpanded and forbidden calls shall retain ordinary
procedure records. Products without required-inline callables keep the existing partitioned O0
path; this restriction shall not globally disable parallel code generation.


## NSC-MEM-009 — Volatile MMIO field lowering

The lexer and parser shall recognize `volatile` as a field-only modifier. Member analysis shall
accept instance and static fields only when their type is a non-owning native scalar, enum, or raw
pointer. It shall reject aggregate and managed-reference types, ownership-bearing storage,
readonly/const combinations, duplicate modifiers, and placement on any non-field declaration.

The volatile bit shall be carried explicitly by the member model, HIR address/load nodes, MIR
address/load nodes, and static-field records. Dumps shall expose the bit. A later stage shall never
re-infer volatility from source spelling or from a type name.

Every direct field load and every store whose target is a volatile field shall call the LLVM C API
volatile marker on the exact load/store instruction. Compound assignment and increment/decrement
shall mark both the read and the write while evaluating the address once. Ordinary neighboring
storage must remain ordinary. This contract does not authorize `atomic`, `atomicrmw`, `cmpxchg`,
`fence`, or a runtime helper.

NSMETA 1.14 shall preserve the bit on both the public member record and its physical layout-field
record. Public-source reconstruction shall reproduce the modifier for instance and static fields,
so a `LibraryReference` consumer emits the same access contract. A requested NSMETA 1.13 or older
artifact shall strip the bit rather than attach undefined semantics to a historical version, and
the current verifier shall reject a volatile bit attached to a non-field member.

Focused O0 evidence shall cover instance and static scalar fields, pointer-member syntax, ordinary
and compound reads/writes, verifier-clean LLVM IR, metadata round-trip and down-level stripping.
It shall also prove the absence of atomic instructions and fences and diagnose invalid types and
modifier placement.

## NSC-MEM-010 — Native 64-bit atomic certification

Every `AtomicInt64` core binding shall retain `builtin:long` for its storage, operands, result, and
constants. Ordered load/store shall emit LLVM i64 atomic instructions; ordered increment,
decrement, add, and exchange shall emit i64 `atomicrmw`; compare-exchange shall emit i64 `cmpxchg`.
Each instruction shall declare natural alignment 8.

Legacy overloads shall preserve sequential consistency through the existing runtime hooks. The
Windows x64 runtime shall implement those hooks with native LLVM atomic operations rather than
requiring non-existent Kernel32 `Interlocked*64` exports or an external `__atomic_*` library.

Lowering shall preserve NativeSharp's defined two's-complement wrap at the signed 64-bit boundary.
It shall preserve the API return contract independently of memory order: increment/decrement/add
return the new value, while exchange and compare-exchange return the previous value. A failed
compare-exchange returns the observed value and leaves storage unchanged.

Focused O0 evidence shall inspect verifier-ready LLVM for i64 width, all operation families,
eight-byte alignment, and absence of external atomic helpers. It shall execute boundary values,
negative operands, successful and failed compare-exchange, all five public orderings, and both
legacy and ordered overloads. This clause certifies the existing implementation; it does not create
a duplicate 64-bit atomic surface.

## NSC-MEM-011 — Native pointer atomics

The compiler shall recognize the exact `NativeSharp.Core.Threading.AtomicPointer` Load, Store,
Exchange, and CompareExchange signatures over `builtin:void*`. It shall synthesize the address of
the receiver's private `value_` field once, retain a pointer type through HIR and MIR, and lower the
operation without converting the published address to an integer carrier.

Legacy overloads shall use sequential consistency. Ordered overloads shall consume compile-time
`MemoryOrder` constants and apply the same legality matrix as integer atomics. A runtime-selected
order, Release load, Acquire store, Release/AcquireRelease failure order, or failure order stronger
than success shall be diagnosed before LLVM emission.

On the Windows x64 contractual target, pointer load/store shall become LLVM atomic pointer
instructions, exchange shall become pointer `atomicrmw xchg`, and compare-exchange shall become
pointer `cmpxchg`. Each instruction shall declare alignment 8. Exchange and compare-exchange shall
return the old pointer; a failed compare-exchange shall leave storage unchanged and return the
observed pointer.

No operation shall infer pointer arithmetic or pointee ownership. Null shall remain a valid value.
Lowering shall require neither `ptrtoint`/`inttoptr` round-trips, Kernel32 pointer-interlocked
exports, nor an external `__atomic_*` helper library.

Focused O0 evidence shall cover legacy and ordered overloads, all five public orders where legal,
null, pointer identity, successful and failed compare-exchange, invalid-order diagnostics,
verifier-ready LLVM, natural alignment, linking, and execution.

## NSC-MEM-012 — Public Interlocked API

The compiler shall recognize only the exact public static
`NativeSharp.Core.Threading.Interlocked` signatures whose first parameter is `ref int`,
`ref long`, or `ref void*`. The first argument shall lower as the address of the caller's
addressable storage and shall be evaluated exactly once.

Interlocked shall reuse the existing integer and pointer atomic core operations. The frontend shall
not synthesize a wrapper object or private field, and MIR/LLVM shall not contain a second atomic
implementation selected only because the source call was static.

Legacy overloads shall retain sequential consistency. Ordered overloads shall fold `MemoryOrder`
at compile time and enforce the existing load, store, RMW, and independent CAS success/failure
rules before LLVM emission. Omitting `ref`, passing non-addressable storage, selecting an order at
runtime, or using an illegal order shall be rejected with a source diagnostic.

The integer overloads shall preserve their existing width, alignment, wrapping, and return
contracts. Pointer overloads shall remain pointer-typed and naturally aligned without ownership,
pointer arithmetic, integer round-trips, Win32 imports, or external atomic helpers.

Focused O0 evidence shall exercise all three location families, legacy and ordered operations,
successful and failed compare-exchange, pointer identity and null, static ref-address lowering,
invalid orders, missing `ref`, verifier-ready LLVM, linking, and execution.

## NSC-REF-001 — Reference-local representation and lowering

The parser shall recognize `ref T name = ref expression;` as a local declaration while preserving
`ref` as a contextual identifier in every non-declaration shape. Both `ref` spellings and an
explicit type are mandatory. `const`, `raii`, `owned`, and `borrowed` shall not combine with a
reference local.

Semantic analysis shall retain `T` as the local's logical expression type and separately mark the
local as reference storage. The initializer must be a mutable addressable location of exactly `T`;
implicit numeric, pointer, reference, and user-defined conversions are forbidden. Literals,
temporaries, readonly locations, and properties shall not be materialized into hidden storage.

HIR and MIR shall carry an explicit reference-local flag on the declaration. LLVM shall allocate a
pointer-sized slot holding the source address. An address-context use shall load and forward that
pointer; a value-context use shall additionally load through it. Assignment, increment, field or
element aliasing, alias chaining, and forwarding to a `ref` parameter must therefore observe one
underlying location without a new runtime helper or ownership operation.

Capturing a reference local in a lambda shall be rejected until escape and lifetime analysis can
prove the delegate cannot outlive the referenced storage. Focused O0 evidence shall cover semantic,
HIR, MIR, verifier-ready LLVM, linking, real execution, exact-type diagnostics, mutable
addressability, explicit syntax, and capture rejection.

## NSC-REF-002 — Reference-return ABI and lowering

The parser shall recognize `ref` before an ordinary method or function return type and after
`return`, while retaining it as a contextual identifier elsewhere. Semantic analysis shall require
`return ref` in a ref-returning callable and mutable addressable storage of the exact declared type.
It shall reject ordinary frame locals, value and `in` parameters, temporaries, ownership-qualified
returns, lambdas, and `extern C` reference-return boundaries.

Calls to ref-returning functions shall be addressable and assignable. HIR and MIR shall carry
explicit facts for both the callable ABI and each call result. LLVM shall physically return `ptr`;
address contexts shall preserve it and value contexts shall load through it. Direct static,
instance, virtual, and interface calls shall use the same rule without `sret` substitution.

Override and interface matching shall include the ref-return mode as well as exact logical type.
NSMETA 1.15 shall encode the mode on public methods, strip it when deliberately emitting an older
minor, reconstruct it for `LibraryReference`, and retain the producer's physical symbol identity.
Focused O0 evidence shall cover reads, writes, forwarding, virtual/interface dispatch, diagnostics,
verifier-ready LLVM, linking, execution, metadata round-trip, downlevel stripping, and a real
producer/consumer archive boundary.

## NSC-REF-003 — Reference-indexer accessor contract

The parser shall accept `ref` on class, struct, and interface indexer declarations, copy the mode to
the synthesized `get_Item`, and reject a setter on the same declaration. The getter shall obey the
exact-type, mutable-address, ownership, and lifetime rules of NSC-REF-002.

Semantic analysis shall distinguish a reference getter from the established getter/setter rewrite.
Its index expression shall be assignable and addressable without resolving `set_Item`; ordinary
indexers shall remain non-addressable and require an accessible setter for assignment. HIR shall
lower the receiver and indices once, preserve the returned address for stores and reference-local
binding, and load it only in value contexts.

Virtual and interface dispatch shall preserve the reference-result fact. NSMETA 1.15 source
reconstruction shall emit a get-only reference indexer when `get_Item` carries the reference-return
bit and reject inconsistent metadata that combines that bit with a property or setter. Focused O0
evidence shall include direct, compound, reference-local, virtual, interface, linked execution, and
precompiled producer/consumer cases.

## NSC-ESCAPE-001 — Unified reference escape-origin analysis

The method-body model shall assign every reference local one explicit origin classification:
external/caller storage, current-frame local, current-frame value parameter, or VLA storage. It
shall compute the classification once at binding and preserve it through chained reference locals.

Reference-return validation shall query that model rather than inspecting only the final syntax.
Origin propagation shall cover value-struct fields, fixed/flexible inline elements, ref indexer
receivers, and ref-return calls whose ref/out arguments may be forwarded. Returning either direct
or transitively rooted current-frame storage shall produce a source diagnostic; ref/out caller
storage, static storage, and raw-pointer pointees without known VLA provenance shall remain valid.

The method-body dump shall expose the retained origin for auditability. Focused O0 evidence shall
exercise safe caller/static/pointer forwarding and reject direct, chained, call-forwarded,
value-parameter, struct-field, and fixed-array frame escapes. Existing VLA, borrow, and view escape
suites shall remain green as adjacent lifetime domains.

## NSC-LIFETIME-REF-001 — Reference lifetime regions

The method-body model shall attach a maximum valid lexical scope to every known reference origin
and the declaration scope to every reference local. The maximum shall be the source local or VLA
scope for current-frame storage and the method root for parameters. Unknown raw-pointer pointee
provenance shall retain the explicit unsafe contract rather than inheriting the pointer slot's
lifetime.

Reference-local binding shall require the alias scope to equal or descend from the maximum valid
scope. Address-preserving aliases, aggregate members, indexing and ref-return calls shall propagate
the original maximum without widening it. Reference-return validation shall continue to treat the
return boundary as outside every current-frame region, while ref/out caller storage remains valid.

The method-body dump shall expose both `max-scope` and `alias-scope` for auditability. Focused O0
evidence shall cover nested aliases of caller and local storage, linked execution, and transitive
rejection of nested locals, struct reference indexers and VLA aliases. Existing reference-return,
reference-indexer, VLA, borrow and view suites shall remain green. The analysis shall add no runtime
helper, ownership operation, metadata field or ABI change.

## NSC-PLACEMENT-001 — Placement-construction syntax boundary

The parser shall recognize `new (storage) T(arguments)` as a dedicated
`PlacementConstructionExpression`. Its first operand shall retain the complete storage expression,
its type syntax shall retain `T`, and its children shall retain constructor arguments in source
order, including argument names and pass modes. Both parenthesized groups are mandatory.

The new form shall not alter the established AST identity of ordinary object, array, or raw-storage
allocation. The syntax-only frontend may retain `PlacementConstructionNotAvailable`; the project
pipeline shall implement the expression according to NSC-PLACEMENT-002. Focused syntax evidence
shall cover the AST shape, a compound destination expression, ordinary-`new` non-regression,
missing argument-list rejection and the explicit standalone-frontend boundary.

## NSC-PLACEMENT-002 — Supplied-storage construction lowering

Project semantic analysis shall require an unmodified concrete class or struct `T` and exact
destination type `T*`. It shall assign result type `T*` for structs and the ordinary non-owning
class-reference type `T` for classes. Constructor resolution shall reuse the established candidate,
conversion, named/default argument, accessibility and ownership machinery.

HIR shall carry a dedicated placement node whose first child is the destination, followed by the
source-ordered constructor arguments and any synthesized initializer work. MIR shall freeze the
canonical target byte size and alignment without creating an allocation plan. LLVM shall evaluate
the destination once, then each argument once from left to right, guard against null, zero the
supplied bytes, install a class descriptor when applicable, invoke the selected constructor with
the destination as `this`, and return that same address.

No placement path may call `__nativesharp_alloc`, acquire storage ownership, schedule deallocation,
or manufacture a second object. Pointer capacity, writability and alignment remain explicit unsafe
caller obligations. Focused O0 evidence shall cover explicit and implicit-default struct
construction, class descriptor/member behavior, destination/argument order, HIR/MIR audit nodes,
verifier-clean LLVM, linked execution, and invalid destination/type/constructor diagnostics.

## NSC-PLACEMENT-003 — Object-only destruction lowering

The parser shall recognize `destroy unary-expression` as `DestroyExpression`. Project semantic
analysis shall accept class/interface references and pointers to concrete classes or structs, set
the result type to `void`, and reject RAII locals, owned values, invalid scalar/pointer targets and
recursive `destroy this` from a destructor.

HIR `object-destruction` and MIR `object-destroy` shall preserve whether runtime class dispatch is
required. A class descriptor shall append an object-only `void(ptr)` entry after all established
fields; the pre-existing destruction entry shall call that object-only traversal and then release
the outer allocation. Object-only traversal shall execute the same most-derived-to-base destructor
and owned-field contract, delegate across precompiled bases through the corresponding object-only
entry, and never call `__nativesharp_free` for the outer object. This descriptor expansion is
Object ABI version 2.

LLVM shall evaluate the operand once, treat null as a no-op, dynamically load the object-only entry
for class/interface objects, and emit no machine operation for a trivially destructible struct.
Focused O0 evidence shall cover dynamic derived/base order, reusable caller storage, class-reference
and raw-class-pointer operands, trivial struct lifetime termination, HIR/MIR/LLVM identities, and
invalid scalar, primitive-pointer, and RAII operands.

## NSC-PLACEMENT-004 — Independent object and storage ownership

Project semantic analysis shall attach separate `object-lifetime ownership` and `storage ownership`
facts to expression results and local symbols. Ordinary class construction shall be `owned/owned`;
placement construction shall be `owned/borrowed`; stack allocation, native array allocation and raw
allocation shall be `none/owned`. Local propagation, address-preserving explicit casts, safe casts,
moves, parameters and declared-ownership call results shall retain the applicable pair.

HIR and MIR shall carry the pair explicitly on lifetime-relevant nodes and local declarations.
Object-only destruction shall be `owned/borrowed`; dynamic deletion shall be `owned/owned`; raw
storage release shall carry storage ownership without manufacturing object ownership. The MIR
schema minor shall advance to 1.4 so late consumers can reject an older representation instead of
silently merging the two obligations. Audit dumps shall expose both axes by stable names.

Focused O0 evidence shall cover stack storage, cast provenance, placement construction, ordinary
heap construction, object-only destruction, full deletion and semantic/HIR/MIR preservation. This
contract adds no runtime or Object ABI field and does not make supplied storage owned by placement.

## NSC-NOHEAP-001 — Direct no-heap contexts

The lexer shall reserve `noheap`; the parser shall accept it once on an ordinary class or struct
method, constructor, and interface method contract, and reject duplicate or other placement. The
project member model and its audit dump shall preserve `isNoHeap` independently from inlining,
ownership, ABI and accessibility.

HIR construction shall reject any direct heap-producing node inside a `noheap` body before MIR or
LLVM can be emitted. The closed direct set is heap-backed class `ObjectConstruction` after the
NSC-ESCAPE-002 proof, `RawAllocation`,
`ArrayAllocation`, allocating string construction, `DelegateCreate`, `LambdaCreate`, and
`EventAdd`. `StackAllocation`, `PlacementConstruction`, value construction and object-only
destruction remain legal. Diagnostics shall identify both the callable and the heap-producing HIR
operation at the original expression range. A class construction proven stack-promoted by
NSC-ESCAPE-002 is no longer heap-producing and is permitted.

This item establishes the local body boundary only. Call-graph propagation, override/interface
compatibility and precompiled transport belong to NSC-NOHEAP-002/item 322. No runtime, NSMETA or
Object ABI change is introduced here.

## NSC-NOHEAP-002 — Verifiable no-heap call graph

The parser and member model shall accept `noheap` on ordinary methods, interface method contracts,
and constructors. HIR construction shall require every statically selected call or constructor
reached from a `noheap` callable to carry the same flag. Placement and value construction shall
require an explicit selected `noheap` constructor; an implicit default has no effect contract and
shall be rejected. Constructor initializers and synthesized instance-field initializers execute in
the constructor's effect context and shall be checked there.

Virtual dispatch shall reject an override that drops an inherited `noheap` guarantee. Interface
planning shall reject an implementation or redeclaration that weakens a `noheap` method contract.
Delegate and event invocation from `noheap` shall be rejected until their type signatures acquire
an effect channel; target uncertainty may not be treated as proof.

HIR and MIR functions shall expose the flag. MIR schema 1.6 introduced it and current schema 1.7
shall retain it. NSMETA 1.16 shall
encode the flag on public method and constructor records, validate its callable-only placement,
reconstruct it in a `LibraryReference`, and strip it when intentionally writing 1.15 or older.
This additive semantic contract changes neither native ABI, Object ABI, layout nor runtime symbols.

## NSC-ESCAPE-002 — Stack allocation of nonescaping classes

After a function's complete HIR has been formed, the compiler may promote an exact local
`raii` class construction from heap storage to frame storage only after proving that no use exposes
the local's object identity. Field-value loads and direct field-address stores are the closed safe
use set. Every other local-value, local-address, move, borrow, call receiver/argument, comparison,
capture, return or reassignment shall conservatively decline promotion without diagnosing an
otherwise valid heap program.

The initial eligibility set shall require a source-local root class, a direct zero-argument
`ObjectConstruction`, an implicit default with no constructor entry, no synthesized instance
initializer work, no explicit destructor or owned instance field, canonical nonzero layout, and an
object size no greater than one mebibyte. The declaration and construction shall retain owned
object lifetime and expose storage ownership `none` plus `stack-promoted=true` in HIR and MIR.
MIR schema 1.6 introduced this bit and the construction's destination local identity; current
schema 1.7 shall retain both.

LLVM lowering shall allocate one fixed byte buffer per promoted local in the function entry block,
honor canonical size and alignment, zero and initialize the ordinary class descriptor in that
buffer, and issue no allocator call or allocation-failure branch. RAII shall use descriptor entry
7, the existing object-only destruction entry, instead of entry 5, the destruction-and-free entry.
The pointer owner slot shall retain the existing exactly-once cleanup guard.

A successfully promoted construction is non-null and is eligible within a `noheap` callable. If
the post-HIR proof declines it, the existing direct `ObjectConstruction` heap diagnostic remains
mandatory. This compiler transformation changes no parser surface, NSMETA, class layout, Object
ABI, Runtime ABI, bindings, or runtime implementation.

## NSC-LAYOUT-002 — Anonymous struct fields

The parser shall recognize `struct { members } name;` in a class or struct member position without
requiring or manufacturing a source-level type name. It shall retain one compiler-generated nested
`StructDeclaration` with a deterministic identity derived from file identity and source offset, ordinary body
members as children, and the containing `FieldDeclaration` on a separate AST edge. A present field
name shall retain qualified access; an omitted declarator shall follow NSC-LAYOUT-004.

Name lookup shall predeclare the synthesized struct through the existing nested-type path. Member
collection shall assign body members only to that synthesized type and the carrier field only to
the containing type. Duplicate checking, accessibility, canonical type keys, layout, member access,
HIR, MIR and LLVM shall then use their ordinary named-struct machinery; no parallel anonymous type
category or runtime representation is permitted.

Layout modifiers before `struct` shall apply to the embedded value layout, while an explicit field
offset applies to the carrier. Recursive anonymous structs shall remain structurally distinct.
The value shall occupy inline storage and shall introduce no descriptor, allocation, ownership,
Object ABI, Runtime ABI or MIR-schema change.

## NSC-LAYOUT-003 — Anonymous union fields

The parser shall recognize contextual `union { members } name;` in a class, struct or union member
position without requiring a source-level type name. It shall retain a compiler-generated nested
`StructDeclaration` whose deterministic identity derives from file identity and source offset,
mark it with ordinary union layout semantics, keep body fields as its children and attach one
ordinary carrier `FieldDeclaration` on a separate AST edge. A missing carrier name shall follow
NSC-LAYOUT-004.

Name lookup and member collection shall follow NSC-LAYOUT-002. The object-layout model shall be the
only downstream component that distinguishes the synthesized union from a struct: every instance
alternative begins at offset zero and size and alignment cover the largest alternative under the
existing `pack`, `align` and asserted-size rules. An explicit field offset applies only to the
carrier. Member access, HIR, MIR and LLVM shall reuse ordinary nested value-field lowering.

The representation shall contain no hidden discriminant, active-member state, descriptor,
allocation or ownership. This feature changes no Object ABI, Runtime ABI, MIR schema, runtime or
binding surface.

## NSC-LAYOUT-004 — Anonymous-member promotion

For an unnamed `struct { fields };` or `union { fields };`, the parser shall keep the synthesized
nested aggregate and one compiler-generated physical carrier field, marking both deterministically.
The member model shall add separate nonphysical aliases on the containing type for instance fields
only. Promotion shall recurse through unnamed carriers, retain the ultimate source field and its
ordered carrier path, compute conservative effective accessibility, and diagnose collisions with
direct or previously promoted names. Named carriers shall not promote.

The layout model shall first complete every physical aggregate and then derive each alias offset by
adding its carrier-path offsets to the ultimate source offset. Aliases shall not appear in physical
field lists and therefore shall not duplicate storage, initialization, destruction, header state or
MIR operands. Member access, assignment and `offsetof` shall resolve through the derived layout.

The C header generator shall spell promoted carriers as C/MSVC anonymous structs or unions, omit
compiler-generated identities and assert promoted offsets. NSMETA 1.17 shall mark physical
anonymous carriers and its source materializer shall recursively reconstruct them for a
`LibraryReference`. Writing minor 1.16 or earlier shall clear this unsupported marker. This feature
did not itself require a MIR schema change; current MIR 1.7, Object ABI 2, Runtime ABI and runtime
ownership remain governed by their own contracts.

## NSC-LAYOUT-005 — Bit-field syntax boundary

The parser shall accept `type name : width;` and `type : width;` as field declarations. It shall
retain the complete width expression on `FieldDeclaration::second()`, independently from the
ordinary initializer edge, mark the node as a bit field, and synthesize a deterministic unreachable
name for unnamed declarations. Assignment expressions used as initializers shall diagnose rather
than become widths. Bit fields may carry accessibility and `volatile`; storage, callable, explicit
byte-layout and type-layout modifiers shall diagnose at this boundary.

The member model shall preserve `isBitField`, `isUnnamedBitField` and the width syntax pointer while
keeping unnamed identities compiler-generated. Until NSC-LAYOUT-006 implements MSVC storage units,
both struct and class layout shall reject such a member with the explicit item-328 boundary. No
pipeline may silently allocate the declared integer type as an ordinary full-width field. Item 327
changes no ABI schema, metadata format, generated header, runtime or binding.

## NSC-LAYOUT-006 — MSVC bit-field allocation units

`TargetDataLayout` shall be the single authority for physical bit-field placement. Each request
shall retain the declared storage size, effective alignment, optional width and unnamed category;
each result shall publish allocation-unit byte offset, full unit size, bit offset and width.
Equal-sized consecutive units may share storage while the next field fits. A size change, exhausted
unit, ordinary field or effective zero-width barrier shall close the active unit. `pack` may cap
alignment but shall not shrink the declared allocation unit.

A zero-width field shall be unnamed. It shall have no effect without a live preceding bit-field
unit; otherwise it shall close that unit, align the next position to its effective base-type
alignment and contribute that alignment to the aggregate. Union bit fields shall all begin at byte
and bit zero, contribute their full units to extent and not independently raise union alignment.
Class layout shall invoke the same target authority after representing its header or completed base
subobject as an immutable prefix.

The frontend shall accept only integer, bool and enum bases and integral constant widths in range.
The layout dump shall expose bit coordinates for auditing. `offsetof` and HIR field access shall
reject bit fields until item 329 supplies executable masks and sign extension.

The C-header generator shall reproduce named, unnamed and zero-width MSVC declarations and shall
not emit `offsetof` assertions for a bit field. NSMETA 1.18 shall serialize field flags, byte/bit
coordinates and width; writers shall reject a lossy downgrade to 1.17 or earlier, and the public
source materializer shall reconstruct the declarations for `LibraryReference` consumers.

## NSC-LAYOUT-007 — Executable bit-field access

The method-body model shall distinguish assignability from addressability: a mutable named bit
field is an assignment target but cannot satisfy address-of, `ref`, reference-return, inline-assembly
memory-operand or `offsetof` contracts. Unnamed fields shall not consume aggregate-initializer
positions. HIR shall retain the resolved member and evaluate its receiver once.

MIR schema 1.7 shall expose distinct `BitFieldLoad` and logical `BitFieldAddress` nodes carrying the
allocation-unit byte offset, alignment and size plus bit offset and width. The logical address node
may only be consumed by ordinary or compound stores; it does not promise a separately addressable
object. Imported NSMETA 1.18 layouts shall produce the same MIR coordinates as local declarations.

LLVM lowering shall load the declared integer allocation unit, shift and mask the slice, and use
the declared base or enum-underlying signedness for extension. Bool reads shall compare the slice
with zero. Stores shall mask the incoming low bits, clear only the destination slice, merge it into
the previously loaded unit and store the complete unit. Assignment expressions shall return the
post-truncation logical value. Compound stores and prefix/postfix updates shall reuse the same
typed arithmetic path while evaluating the receiver once. Aggregate initialization shall zero the
carrier first and insert each named bit field with the same merge operation.

This schema change does not alter NSMETA 1.18, Object ABI 2, Runtime ABI 1.0 or ownership.

## NSC-LAYOUT-008 — Volatile bit-field lowering

HIR and MIR shall preserve the resolved field's `volatile` qualifier on bit-field loads, logical
addresses and aggregate-element stores. A read shall issue one LLVM volatile load of the declared
allocation-unit integer and extract only afterward. A simple or compound write shall issue a
volatile load and a volatile store of that same complete unit around the ordinary masked merge;
prefix/postfix result rules and single receiver evaluation remain those of NSC-LAYOUT-007.

The compiler shall not translate volatile bit fields into narrow independent accesses, atomics or
fences. O0 and O2 shall preserve the volatile operations. NSMETA 1.18 reconstruction shall combine
the existing volatile member flag with the stored bit coordinates so local, ProjectReference and
LibraryReference declarations lower identically. MIR remains schema 1.7 and no ABI version changes.

## NSC-ABI-001 — Variadic C declaration preservation

The lexer shall recognize `...` by maximal munch without changing one- or two-dot tokenization.
The parser shall accept it only as the terminal marker of a body-less imported `extern C`
declaration with at least one fixed parameter. The AST and member model shall retain an independent
variadic bit, and the signature key and imported-C conflict identity shall distinguish fixed and
variadic declarations of the same external symbol.

HIR and MIR shall carry the bit on the body-less import and LLVM shall declare the function with
the written fixed prefix followed by varargs. NSMETA 1.19 shall preserve the bit and generated
`LibraryReference` source shall reconstruct the ellipsis. A pre-1.19 downlevel shall fail rather
than erase it. Variadic calls shall use the promotions in NSC-ABI-002.

## NSC-ABI-002 — C default argument promotions

The call binder shall map ordinary arguments to the written fixed parameters and assign each
subsequent positional argument a stable tail ordinal. It shall reject named tail arguments,
by-reference passing, ownership-bearing values, managed references, arrays, aggregates, and
managed delegates before lowering.

Before HIR, each accepted tail expression shall receive an explicit C default promotion: `float`
to `double`, and `bool`, sub-32-bit integers, and sub-32-bit enums to `int`. Wider enums convert to
their compatible integer type; other 32/64-bit integers, `double`, raw pointers, and `extern C`
delegates retain their type. HIR and MIR
shall retain the promoted operand types and the fixed-prefix-plus-tail argument order; LLVM calls
shall use the variadic function prototype at O0 and O2. Calls through `LibraryReference` metadata
shall behave identically. Physical Windows x64 vararg register certification belongs to
NSC-ABI-003.

## NSC-ABI-003 — Win64 variadic call lowering

For the `x86_64-pc-windows-msvc` target, the LLVM call site shall retain the variadic function type
rather than casting the callee to a fixed prototype. Target lowering shall therefore duplicate
each floating-point value in the first four argument slots into the corresponding XMM and
general-purpose registers, reserve the Win64 shadow space, and place remaining promoted arguments
in eight-byte stack slots.

Certification shall execute NativeSharp callers against independently compiled C11 and C++20
callees that consume the arguments through `va_list`. It shall cover promoted and direct floating
values in register slots, promoted integers in register and stack slots, 32/64-bit integers, raw
pointers, and `extern C` callbacks at O0 and O2. An IR-only assertion is insufficient for this
contract because it does not observe the final machine calling sequence.

## NSC-ABI-004 — Microsoft x64 vectorcall delegates

An `extern vectorcall delegate` shall remain a raw function pointer, use LLVM
`x86_vectorcallcc` on both the published exception-boundary thunk and every indirect invocation,
and keep ordinary NativeSharp calling convention on the thunk-to-body edge. The thunk shall retain
the same exception containment as an `extern C delegate`.

NSMETA 1.20 shall preserve vectorcall as delegate flag bit 1 and reject a lossy pre-1.20
downlevel. Generated C/C++ headers shall identify the function pointer through `NS_VECTORCALL`.
Certification shall execute at least six floating arguments at O0 and O2 so register placement is
observed rather than inferred only from IR text.

## NSC-ABI-005 — Vector and HVA values at a vectorcall boundary

The compiler shall classify `NativeSharp.Core.Numerics.Float32x4` as a direct `<4 x float>` value
at a vectorcall boundary. A sequential, unpacked aggregate of one to four consecutive
`Float32x4` fields shall be represented as a homogeneous LLVM vector struct and carry the Win64
`inreg` parameter contract. Both categories shall return directly rather than through sret.

This classification is boundary-specific. Ordinary NativeSharp bodies and ordinary Win64 calls
shall retain the stable aggregate carrier and pointer/sret ABI. The generated exception boundary
thunk shall bridge both parameter and return representations, while union, explicit-layout,
packed, mixed-field, or more-than-four-member aggregates shall not be promoted to HVA.

## NSC-ASM-001 — Integrated assembly grammar boundary

The parser shall recognize contextual `asm { ... }` only as a statement in function and method
bodies, consume balanced braces, retain an explicit `InlineAssemblyStatement` AST identity, and
resume at the following statement without requiring a semicolon. Namespace-scope use and an
unterminated block shall produce source diagnostics with deterministic recovery.

The standalone frontend may reject the AST node with `InlineAssemblyNotAvailable` while it lacks a
physical backend. The project pipeline shall accept a fully validated node and lower it explicitly;
no pipeline may treat assembly as an expression, silently erase it, or fail later as unknown syntax.

## NSC-ASM-002 — Assembly lexical records and source locations

After contextual `asm` and its opening brace, the lexer shall suspend ordinary NativeSharp token
classification until the balanced outer closing brace. It shall preserve every body byte and the
physical spelling of CRLF, LF, or CR. Characters that are illegal in ordinary NativeSharp source
shall remain assembly text rather than produce an ordinary-language lexical diagnostic.

The parent AST statement shall reconstruct the exact body. Every non-blank physical line shall
produce an `InlineAssemblyLine` whose text excludes surrounding horizontal whitespace and whose
range begins at the first significant byte and ends after the last. Normal lexing shall resume
immediately after the outer brace, and using `asm` without `{` shall remain an ordinary identifier.

## NSC-ASM-003 — Positional input operands

The parser shall accept a non-empty `in(expression, ...)` clause after the assembly block and
require a final semicolon whenever that clause exists. Every expression shall become an
`InlineAssemblyInputOperand` with a stable zero-based ordinal and retain the ordinary expression
AST as its first child. Both semantic pipelines shall bind every input expression before their
respective lowering boundary. An empty clause shall be rejected at its closing parenthesis and
shall not create a phantom operand.

## NSC-ASM-004 — Positional output operands

The parser shall accept a non-empty contextual `out(target, ...)` clause before any input clause.
Each target shall become `InlineAssemblyOutputOperand`, retain a zero-based `$outN` ordinal and keep
its unary/postfix lvalue syntax as `first()`. Both semantic pipelines shall require a modifiable
lvalue. Project definite-assignment analysis shall treat a whole-local output as a write and shall
not diagnose a read of its previous uninitialized value.

## NSC-ASM-005 — Register constraints

The parser shall accept an optional `: "constraint"` after every assembly input expression and
output target. It shall normalize an omitted input constraint to `r` and an omitted output
constraint to `=r`, while retaining an explicitness bit independently from the normalized value.
Inputs may use `r` or `{register}`. Outputs may use write-only `=r`/`={register}` or read-write
`+r`/`+{register}`. Directionally invalid forms, non-literal constraints, empty fixed-register
names, braces with trailing data, whitespace, and punctuation inside a register atom shall produce
`InvalidInlineAssemblyConstraint` at the operand. Architecture-specific register validation is a
separate later contract and shall not be guessed by the parser.

## NSC-ASM-006 — Immediate and memory operand validation

The `i` input constraint shall require an integral `ExpressionResult` carrying a compile-time
integer constant. The `m` input constraint shall require addressable storage. Project expression
analysis shall track addressability independently from assignability so readonly storage can be
distinguished from properties that merely have a setter. Output constraints `=m` and `+m` shall
require both addressability and the existing modifiable-lvalue contract. No validation path may
silently spill a temporary or convert a property access into a memory operand.

## NSC-ASM-007 — Register clobbers

The parser shall accept one optional non-empty `clobber(string-literal, ...)` clause after operand
clauses. Each valid entry shall produce `InlineAssemblyClobber` with a lowercase name and stable
zero-based ordinal. Duplicate names after case normalization, non-literals, malformed atoms, and a
name also selected by a fixed-register input or output constraint shall produce
`InvalidInlineAssemblyClobber`. `cc` and `memory` shall remain reserved until their explicit
semantic contract is implemented; architecture-specific register availability remains separate.

## NSC-ASM-008 — Condition-code and memory clobbers

The clobber parser shall recognize canonical `cc` and `memory` entries as reserved effects rather
than physical registers. `InlineAssemblyClobber::auxiliaryText()` shall classify physical names as
`register`, `cc` as `flags`, and `memory` as `memory`. Case normalization, stable ordering, and
duplicate detection shall remain identical across all categories. Later MIR and LLVM stages shall
consume the category instead of reinterpreting the source spelling.

## NSC-ASM-009 — Stack and nonvolatile-register safety

The parser shall reject stack-pointer aliases `rsp`, `esp`, `sp`, and `spl` in both fixed operand
constraints and clobbers. It shall mark Windows x64 nonvolatile GPR and XMM clobbers through the
clobber AST flag. Later lowering must translate that declaration into backend-visible preservation;
it may not trust authored assembly to save and restore ABI state implicitly.

## NSC-ASM-010 — Local assembly control flow

The parser shall collect labels before resolving branches so forward targets work. Labels shall be
unique inside one block after case normalization. Direct x64 branch mnemonics may target only those
labels. External or indirect branches and call/return instructions shall produce
`InvalidInlineAssemblyControlFlow`. Resolved lines shall carry stable label/branch AST metadata.

## NSC-ASM-011 — x64 register and CPU-feature validation

`TargetInfo` shall own the assembly target contract: a named CPU baseline, its guaranteed features,
the valid physical-register catalog, and instruction-to-required-feature classification. The parser
shall reject unknown fixed/clobber registers and instructions whose required feature is absent.
Diagnostics shall identify the spelling, missing feature, and selected baseline before HIR. The
contract shall be deterministic and independent from the compiler host CPU.

## NSC-ASM-012 — Structured HIR and MIR assembly contract

The project pipeline shall lower each validated statement to one `InlineAssembly` HIR operation
and one corresponding MIR operation. The operation shall preserve exact body bytes. Ordered child
records shall retain instruction spelling and source identity; operand direction, ordinal,
normalized constraint, explicitness, resolved type, and lowered value or address; and clobber
ordinal, category, canonical name, and nonvolatile-preservation marker.

Memory inputs and every output shall carry storage addresses; register and immediate inputs shall
carry values. HIR-to-MIR conversion shall not reparse assembly text or infer constraints from it.
The schema revision introducing these node meanings is MIR 1.1. Executable LLVM emission remains a
separate contract and absence of that later layer must never erase or mutate this representation.

## NSC-ASM-013 — LLVM inline-assembly lowering

The x64 backend shall emit structured MIR assembly through LLVM inline asm in Intel dialect. It
shall map NativeSharp `$outN` and `$N` placeholders into the combined LLVM operand space, diagnose
undeclared or malformed placeholders before HIR, and preserve source operand evaluation order.

Register outputs shall be returned by the inline-asm call and stored to their validated addresses;
multiple register outputs shall use an aggregate result. Read-write outputs shall supply their
incoming value. Memory operands shall use indirect constraints and carry the required `elementtype`
call-site attribute. Canonical register, `cc`, and `memory` clobbers shall become LLVM clobbers.
Local labels shall receive a per-expansion unique identity so repeated or inlined blocks cannot
collide. The configured x86 target shall initialize both its assembly parser and printer, and a
focused O0 test shall emit a verifier-clean COFF object rather than only inspect textual IR.

## NSC-ASM-014 — Optimization barriers and effect summaries

Every assembly statement shall be represented and emitted as side-effecting, including a block
with no operands or clobbers. MIR schema 1.2 shall carry explicit `hasSideEffects`, memory-clobber,
and condition-code-clobber summaries derived from the validated clobber records. LLVM lowering
shall reject any disagreement between those summaries and the records instead of weakening an
effect silently.

A `memory` clobber shall become LLVM's broad memory clobber and prevent optimization from deleting,
merging, or moving ordinary memory operations across the block. A `cc` clobber shall expose flag
destruction. In the absence of `memory`, only declared operands and other clobbers constrain the
optimizer; assembly is not an implicit full memory fence. Focused O2 evidence shall retain an
operand-free block and stores on both sides of a memory-clobber block in source order.

## NSC-ASM-015 — CodeView locations for assembly instructions

When either `-g` or `-gline-tables-only` is active, LLVM lowering shall prefix every physical
non-blank assembly source line with a generated CodeView location for that line and column. The
file number shall identify the same checksummed and prefix-remapped `DIFile` used by the enclosing
NativeSharp function. `-g0` shall add no debug directive.

The backend shall retain one inline-assembly operation for the complete source block. It shall not
split lines into independent calls merely to manufacture stepping records, because doing so can
change register continuity, outputs and local control flow. Focused O0 evidence shall link a
standard PDB, execute the program, and show distinct addresses for consecutive instruction lines.

## NSC-ASM-016 — Independent C and C++ ABI certification

An `export extern C` function containing integrated assembly shall retain the ordinary generated C
ABI contract. Its generated header and import library shall be sufficient for independent strict
C11 and C++20 hosts to compile, link and execute without NativeSharp source or a private adapter.

Focused Windows x64 evidence shall cover scalar register inputs and outputs, an observable write
through a raw pointer, and a physical nonvolatile-register clobber. The last case shall modify `rbx`
inside NativeSharp assembly while independent callers keep a sentinel live across the call; both
callers must observe it unchanged. Successful return values alone do not replace this preservation
check. The certification uses O0 and the real DLL/import-library/header product path.
