# T7 - Access checked on the selected C++ overload

## Defect

C++ resolves overloads over every candidate and checks access only on the one it selects
([over.match]/3, [class.access]). NativeSharp ranks only accessible members, so with the C++ arithmetic
conversions of T1 and T2 an accessible overload could be chosen where C++ chooses an inaccessible one and
rejects the call (`pick(1.5)` with a public `pick(int)` and a private `pick(double)`). T1 and T2 avoided it
conservatively: a candidate that needed such a conversion was not viable whenever an inaccessible candidate of
the same name (or a constructor of the same arity) existed, even when that candidate could never be selected.

## Change

The conservative rules are removed (`rejectCppArithmeticBesideInaccessible`,
`ConstructorCandidateBinding::usesCppArithmeticConversion`, `MethodCallCandidate::usesCppArithmeticConversion`,
`ExpressionResult::inaccessibleNamedMember`) and replaced by C++'s order:

- Constructors: `evaluateConstructorCandidate` (`ConstructorCandidateEvaluation.cpp`) also evaluates an
  inaccessible C++ constructor (`isCppExternal`); its `viable` reflects its arguments and `accessible` stays false.
  The existing ranking (`nonDominatedConstructorCandidates`) orders accessible and inaccessible candidates
  together. `rejectInaccessibleSelection` (`ConstructionBinding.cpp`) rejects the construction when the single
  best candidate is inaccessible, in `resolveConstructionCandidates` (resolution `Inaccessible`) and in the
  `this(...)`/`base(...)` initializer path (`ConstructorInitializers.cpp`). `previewSelectedCandidate` previews
  no constructor in that case.
- Methods: `collectAccessibleMemberCandidates` (`MemberAccessAnalyzer.cpp`) and
  `collectIdentifierMemberCandidates` (`IdentifierAnalyzer.cpp`) keep the inaccessible C++ methods they skip in
  `ExpressionResult::inaccessibleCppMethodCandidates`. `evaluateCallCandidates` (`CallAnalyzer.cpp`) evaluates
  them after the accessible ones, marked `MethodCallCandidate::inaccessible`, so they take part in arity,
  viability, the ref-qualifier filters and `chooseNonDominatedCallCandidate`. When the single best candidate is
  inaccessible the call is rejected with `InaccessibleMember`; a tie that includes one stays an ambiguity.

## Preserved behavior

- NativeSharp constructors and methods: inaccessible ones are still not candidates, as in C#.
- C calls and namespace functions (always public): unchanged.
- An accessible C++ overload that is better than every inaccessible one is selected as before; T1 and T2's
  conversions keep their rank 5.
- The resolution messages for the cases without inaccessible C++ overloads are unchanged.

## Pending verification

- No tests, baselines or probes were run, as requested. Pending: the access case of pruebas-tanda-10-acceso
  (expected: "the C++ overload of 'pick' selected for these arguments is not accessible in this context"), a
  private C++ constructor beaten by none, and the tutorials. Diagnostics that list candidates may now also list the
  inaccessible C++ overloads, as Clang does.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0). The build is not a functional
  proof.
