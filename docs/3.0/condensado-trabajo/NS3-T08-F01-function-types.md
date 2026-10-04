# NS3-T08-F01 — Complete function types

Status: implemented on `development`  
Platform: Windows x64  
Validation profile: directed O0/NSLink only; no baseline

## Delivered contract

NativeSharp 3 now has productive structural function types across parser, name/type binding,
generic specialization, overload resolution, ownership/HIR, NSMETA, LLVM, and execution:

- `fn*` is a raw function pointer; `fn&` is a managed callable reference.
- `native`, `C`, `stdcall`, `fastcall`, and `vectorcall` are part of type identity.
- ordered parameters, result, variadicity, `unsafe`, `noheap`, `async`, typed `throws`, lifetime,
  and nullability are represented by one canonical length-prefixed identity.
- function types work as locals, parameters, returns, fields, overload discriminators, generic
  arguments, DLL callback parameters, and public NSMETA signatures.
- implicit compatibility is effect-aware and directional. Null converts only to an explicitly
  nullable function type. Unsafe invocation requires an explicit unsafe block.
- foreign variadic pointers use fixed-parameter LLVM signatures plus C varargs and the existing C
  default argument promotions.

The normative bilingual contract and compatibility matrix are in
`docs/3.0/language/function-types.md`.

## Implementation changes

- `TypeSyntax` and the parser carry the complete structural syntax, including nested/generic use.
- `CallableSemantics` owns canonical serialization, strict parsing, and assignability.
- name lookup, aliases, opaque aliases, generics, constraints, layout, and conversions recognize
  the structural carrier without inventing a nominal declaration named `$function`.
- method-body binding reuses the mature delegate create/invoke pipeline while preserving the full
  function key; raw pointers and managed references choose their established ownership paths.
- HIR and LLVM recognize `fn&` as a managed reference and `fn*` as a raw pointer. LLVM preserves
  vectorcall and variadic call construction.
- NSMETA dependency collection and verification recurse through parameter, result, and error type
  components, rejecting malformed structural identities.

## Files changed

- `CMakeLists.txt`
- `include/nativesharp/AST/TypeSyntax.h`
- `include/nativesharp/Frontend/ProjectTypeConversion.h`
- `include/nativesharp/Language/CallableSemantics.h`
- `src/AST/AstDumper.cpp`
- `src/AST/TypeSyntax.cpp`
- `src/Frontend/ProjectGenericConstraintModel.cpp`
- `src/Frontend/ProjectGenericModel.cpp`
- `src/Frontend/ProjectMemberModel.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectNameLookup.cpp`
- `src/Frontend/ProjectObjectLayoutModel.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOpaqueResultModel.cpp`
- `src/Frontend/ProjectTypeConversion.cpp`
- `src/Interop/LibraryMetadata.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `src/Language/CallableSemantics.cpp`
- `src/Parser/Parser.cpp`
- `tests/unit/NativeSharp3T08CallableTests.cpp`

## New files and why

- `docs/3.0/language/function-types.md`: normative ES/EN syntax, identity, compatibility, ABI, and
  boundary contract.
- `docs/3.0/items/NS3-T08-F01-function-types.md`: atomic implementation record and evidence for
  this roadmap item.

No new runtime component, library, generated source tree, or persistent temporary directory was
created.

## Concrete improvement

The previous `FunctionType` helper was declarative and nominal delegates were the only productive
callable types. Public signatures could not express pointer/reference carrier, ABI, effects, or
nullability structurally. The compiler now uses one lossless identity from source through NSMETA
and machine code, so overloads, generic instances, cache keys, and consumers cannot silently merge
incompatible callable contracts.

## Directed evidence

- Exact target `nsc_ns3_t08_callable_tests`: built successfully.
- Exact feature executable: `PASS: 17 checks`.
- O0 program linked by NSLink executes raw `fn*` and managed `fn&`, overload selection, generic
  substitution, and distinct mangled targets; process exit is 45.
- Negative compilation rejects unsafe indirect invocation outside `unsafe`.
- NSMETA round-trip preserves structural return/parameter types, foreign variadicity, and a
  `stdcall` callback inside a DLL import signature.
- Exact `nsc` target builds successfully with the static host-runtime contract.

No CTest suite, baseline, O1/O2/O3 build, installation, deploy, remote operation, or write to `C:`
was performed.

