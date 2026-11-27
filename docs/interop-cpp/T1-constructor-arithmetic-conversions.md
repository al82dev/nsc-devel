# T1 - Arithmetic argument conversions for imported C++ constructors

## Defect

An imported C++ constructor rejected an arithmetic argument whose type differs from its parameter, although C++
converts it implicitly:

- `new glm.mat4(1.0)`: a `double` for the `T const&` (`float const&`) scalar constructor.
- The 16-scalar `mat4` constructor (`T const&` parameters) with `double` arguments.

The diagnostic listed the constructors with no viable candidate.

## Cause

`ProjectConstructionModel::evaluateConstructorArgument`
(`src/Frontend/ConstructionModel/ConstructorCandidateEvaluation.cpp`) classified each argument only with
`ProjectConversionClassifier::classifyImplicit`, NativeSharp's own implicit conversions, plus two foreign
exceptions: a C/C++ call result (`classifyForeignCallNumericResult`) and a string literal for `const char*`. It
had no C++ standard arithmetic conversion, so `double` to `float` (a narrowing NativeSharp rejects) never existed
for a C++ constructor. The `const T&` case reached the same classification, with the referent type as the target.

## Change

- New `src/Frontend/CppArithmeticArgumentConversion.h`: `classifyCppArithmeticArgumentConversion` returns a
  `Numeric` conversion of rank 5 between the arithmetic types the C boundary already converts (signed and
  unsigned integers, `nint`/`nuint`, `float`, `double`) and from `bool` to an integer. It is the same set and rank
  as Items 3.3 and L-015 of `classifyCFrontierConversion`.
- `evaluateConstructorArgument`: when no other conversion exists, a C++ constructor (`isCppExternal`) takes that
  conversion for a by-value parameter or for the value bound to a `const T&` or `T&&` parameter (the X1-04 and
  T19-007 materialized value). The candidate records `usesCppArithmeticConversion`.
- `ProjectConstructionModel::rejectCppArithmeticBesideInaccessible`, called by `evaluateConstructionCandidates`
  and `evaluateConstructorInitializerCandidates`: C++ ranks every constructor and checks access afterwards. When an
  inaccessible constructor of the same arity exists, a candidate that needs one of these conversions is not viable,
  so the construction stays rejected instead of selecting an overload C++ would not select. This keeps the
  protection of decision D3 (Item 3.3) for constructors.

Code generation was followed: the selected conversion goes to `ObjectConstructionBinding::selectedConversions`;
`acceptOrdinaryConstructionArgument` (`HirConstructionFrameLowering.cpp`) wraps it with `wrapImplicitConversion`
in a HIR `Conversion` node and, for a `const T&` whose referent is not a record, stores the converted value in a
`TemporaryAddress` whose address the constructor receives. MIR lowers the node to `Convert`
(`MirNodeMachine::lowerGroup15`) and LLVM to `OopLlvmImplementation::convert` / `convertScalar`, the path the C
boundary's `Numeric` conversions already use (`fptrunc` for `double` to `float`). A narrowing integer conversion
follows the surrounding overflow policy, as at the C boundary.

## Preserved behavior

- NativeSharp constructors: only `classifyImplicit`, unchanged.
- Arguments with `ref`, `in` or `out`, parameter-pack elements: unchanged.
- A conversion NativeSharp already accepts keeps its own (better) rank; two C++ constructors that both need an
  arithmetic conversion are still ordered by T19-006 (`compareCppArithmeticSpellings`), and stay ambiguous when C++
  would.
- An integer or floating value still reaches a `bool` parameter only through an explicit cast.
- C calls, `classifyCFrontierConversion`, ownership, NSMETA and Core are not touched.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0).
- No tests, baselines or probes were run, as requested. The construction of the compiler is not a functional
  proof: the capability is pending verification by compiling the tutorials that use these constructors.
