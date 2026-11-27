# T6c - Constructions request constructor template deduction; ranking with non-templates

## Defect

Even with T6a and T6b, nothing asked Clang to deduce a constructor template for a construction, and NativeSharp's
ranking had no C++ rule between a non-template constructor and a constructor template specialization.

## Change

- `ProjectMethodBodyModel::recordCppConstructorTemplateDeduction` (`ConstructionAnalyzer.cpp`), called by
  `recordConstructionSite` (every construction site: ordinary, placement, allocator): for a C++ record with
  `cppHasConstructorTemplates`, a typed construction that is not a factory construction records a
  `CppTemplateDeductionRequest` named `@constructor:<record USR>` with the argument type keys. A `ref`, `in` or
  `out` argument leaves the construction to the non-template constructors.
- `ProjectCompilation::deduceCppTemplateCalls`: such a request never reports a deduction failure (the construction
  keeps its own resolution and diagnostic); an instantiated one sets `cppConstructorTemplatesDeduced_`.
- Deduction round (`compileProjectMethodBodies`): when set, `incorporateDeducedCppConstructors` projects the
  lifetime members again, keeps the `templateSpecialization` ones and appends them
  (`ProjectMemberModel::appendCppLifetimeMethods`, T6b) before the namespace functions and the bodies are rebuilt.
  The request is not repeated in later rounds (`cppDeducedTemplateCalls_`).
- `nonDominatedConstructorCandidates` (`ConstructorCandidateEvaluation.cpp`): with conversions of the same rank for
  every argument, a non-template C++ constructor is better than a constructor template specialization
  ([over.match.best]); otherwise the existing ranking decides (T1 conversions, T19-006 spellings).

## From deduction to the call

1. `recordConstructionSite` records the request; the round calls `deduceFunctionTemplateCallWithExplicit` with
   `@constructor:`, which deduces, orders and instantiates the specialization (T6a).
2. The records snapshot captures it in `deducedConstructors` with its complete-object constructor symbol (T6a);
   `incorporateDeducedCppConstructors` appends it as a `.ctor` member with `cppExternalSymbol` (T6b).
3. The bodies are analyzed again; `evaluateConstructionCandidates` evaluates it with the others; a C++ record
   argument for its `vec<4, U, P> const&` binds as in T3 (a prvalue or a trivially copyable place, materialized).
4. HIR builds its declaration-only function like every imported C++ constructor (`buildDeclarationOnlyFunction`);
   `acceptOrdinaryConstructionArgument` materializes the `const T&` argument (`TemporaryAddress`); MIR lowers the
   construction to `ValueConstruct` (or `PlacementConstruct`) with `functionForMember(...).mangledName`; LLVM
   calls it with the destination first, as any C++ constructor.
5. `emitCppInlineIR` offers the instantiated specialization as `GlobalDecl(constructor, Ctor_Complete)` (T6a).

## Lifetime of the argument temporaries

`glm.inverse(ViewMatrix)` returns a trivially copyable `mat4` by value, stored in a stack temporary of the
function; `[3]` returns a reference into it, so the reference stays valid for the whole construction and the full
expression. A non-trivial prvalue argument is owned by the temporary-ownership pass of `HirExceptionAndCleanup.cpp`
and destroyed at the end of the full expression (T3, T8). Read in the code; not observed in generated code.

## Preserved behavior

- Records without constructor templates: no request, no extra round.
- A construction whose deduction fails keeps its previous resolution and diagnostic.
- Non-template constructors keep their ranking; the new rule only orders a non-template before a specialization on
  equal conversions.

## Known limits

- A specialization deduced for one construction is a constructor of its record for every later construction of it,
  where C++ would deduce again; only identical or NativeSharp-convertible argument types reach it.
- An argument passed with `ref`, `in` or `out` does not request a deduction.
- Records described by a precompiled catalog do not carry `cppHasConstructorTemplates`.

## Pending verification

- Not built: the C++ toolchain at `C:/clang-llvm/bin` that `build-clang` uses was no longer present (see the final
  report). No tests, baselines or probes were run, as requested. Pending: `new glm.vec3(glm.inverse(ViewMatrix)[3])`
  and the other tutorials.
