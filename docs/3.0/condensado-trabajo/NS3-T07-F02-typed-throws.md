# NS3-T07-F02 — Native typed throws

Status: implemented on `development`
Platform: Windows x64
Validation profile: directed O0/NSLink only; no baseline

## Delivered vertical

Edition 3 callables can declare one checked native error effect after their parameter list:

```nativesharp
int Parse(string text) throws ParseError;
```

`throws` is contextual in that position. Edition 2 source may continue to use `throws` as an
identifier; attempting to use the clause itself in edition 2 is rejected by the edition gate.

`E` must resolve to `NativeSharp.Core.Exception` or a class derived from it. A call is legal only
when an active typed/catch-all handler accepts `E`, or when the enclosing callable declares the
same error or a base error. A written `throw value` and `throw;` rethrow obey the same rule.

The contract is supported by methods, free namespace functions, operators, conversion operators,
managed delegates and lambdas targeted to such delegates. Constructors deliberately remain
outside this item; accepting their syntax without full construction-failure ownership semantics
would create a partial contract.

## Subtyping and callable compatibility

- An override or interface/trait implementation may be non-throwing.
- It may narrow `throws BaseError` to `throws DerivedError`.
- It may not introduce an error into a non-throwing contract or widen beyond the inherited error.
- Managed delegate and method-group conversion uses exact error identity. This keeps stored
  callable values deterministic and avoids hidden adapters.
- A lambda inherits the error contract of its target delegate.
- `extern C`, `extern vectorcall`, imported C and exported C callables cannot carry `throws`.
  Native exceptions must be caught and translated by an explicit non-throwing adapter.

## Windows x64 ABI

`throws E` is a checked semantic and callable-type contract. The physical Windows x64 calling
convention remains the existing NativeSharp native exception ABI 1: normal parameters and return
values keep their ordinary Win64 classification, while exceptional control flow uses the compiler
runtime personality and cleanup regions. There is no CLR metadata, managed RTTI, hidden managed
object or implicit result allocation.

The declared error is transported independently through symbols, HIR, MIR schema 1.24, cache
identity and NSMETA 1.37. Changing it changes the published source/API/ABI contract even though it
does not add a normal-path parameter or change the size of a managed delegate cell.

## Cross-module contract

NSMETA 1.37 stores the error type for delegates, members and namespace functions. The reader,
writer, merger, verifier, API diff, ABI diff, ABI report and metadata-backed source materializer all
preserve it. Downgrading a model containing typed throws below 1.37 is rejected instead of silently
erasing the effect. Referenced error types participate in public type closure and materialization.

## `throws` versus `Result`

Use `Result<T,E>` and postfix `?` when failure is ordinary data that callers are expected to inspect,
store or transform. Use `throws E` when failure is nonlocal control flow and native unwind/cleanup is
the intended mechanism. Both are statically explicit, but they are not interchangeable sugar:

- `Result` is an in-band value with variant layout and an ordinary structured return;
- `throws` preserves the ordinary return ABI and has an exceptional control-flow edge;
- `?` never synthesizes a throw;
- a `throws` call never synthesizes a `Result` carrier.

## Files changed

- Parser/AST: `AstNode.h/.cpp`, `AstDumper.cpp`, `Parser.h/.cpp`.
- Binding/symbols: `ProjectGenericModel.cpp`, `ProjectNameLookup.cpp`,
  `ProjectMemberModel.h/.cpp`, `ProjectNamespaceFunctionModel.h/.cpp`.
- Effect checking: `ProjectMethodBodyModel.h/.cpp`, `ProjectVirtualDispatchModel.cpp`,
  `ProjectInterfaceModel.cpp`.
- IR/cache: `ProjectOopHirModel.h/.cpp`, `ProjectOopMirModel.h/.cpp`, their dumpers, member
  dumper and `ProjectOopMirCacheIdentity.cpp`.
- Metadata/materialization: `LibraryMetadata.h/.cpp`, `LibraryMetadataMerger.cpp`,
  `LibraryApiDiff.cpp`, `LibraryAbiReport.cpp`, `Driver.cpp`.
- Directed validation: `NativeSharp3T07ErrorTests.cpp`; version-current assertions in
  `NativeSharp3T06DropGlueTests.cpp` and `Item405FinalCompatibilityCertificationTests.cpp` were
  advanced from NSMETA 1.36 to 1.37 so later aggregate runs do not retain a stale latest-version
  expectation.
- Normative documentation: this file, `language/typed-throws.md`, the T07 documents, roadmap and
  root tracking table.

## New files and purpose

- `docs/3.0/items/NS3-T07-F02-typed-throws.md`: atomic implementation record and evidence.
- `docs/3.0/language/typed-throws.md`: bilingual user-facing normative language contract.

No new runtime library, linker component, helper executable or generated source file was added.

## Directed evidence

- Exact compiler target `nsc`: built successfully during the item.
- Exact feature target `nsc_ns3_t07_error_tests`: built successfully.
- Exact feature executable: `PASS: 22 checks`.
- Productive fixture: O0 emission through NSLink and process exit 42.
- Cross-library fixture: producer `.lib` + NSMETA 1.37, reconstructed consumer contract, native
  exception caught across the library boundary, process exit 42.
- Negative coverage: unhandled call, unhandled throw, delegate mismatch, widening override,
  `extern C` delegate with `throws`, and forbidden NSMETA downgrade. Edition-2 identifier
  compatibility is also exercised.

No CTest suite, matrix, baseline, O1/O2/O3 build, installation, deploy, remote operation or write
outside `A:` was performed.
