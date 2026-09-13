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
inventory is `RuntimeAbiContract.h`, and the runtime-module test shall prove that every listed
symbol is actually materialized.

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

## NSC-ASM-001 — Integrated assembly grammar boundary

The parser shall recognize contextual `asm { ... }` only as a statement in function and method
bodies, consume balanced braces, retain an explicit `InlineAssemblyStatement` AST identity, and
resume at the following statement without requiring a semicolon. Namespace-scope use and an
unterminated block shall produce source diagnostics with deterministic recovery.

Before the assembly operand and lowering contracts are present, both standalone and project
semantic pipelines shall reject the AST node with `InlineAssemblyNotAvailable`. No pipeline may
treat it as an expression, silently erase it, or fail later with an unsupported-node internal error.

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
AST as its first child. Standalone and project semantic analysis shall bind every input expression
before issuing the temporary pre-lowering assembly diagnostic. An empty clause shall be rejected
at its closing parenthesis and shall not create a phantom operand.

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
