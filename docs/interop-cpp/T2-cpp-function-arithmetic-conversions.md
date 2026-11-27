# T2 - Arithmetic argument conversions for imported C++ functions and methods

## Defect

An imported C++ function or method (decorated MSVC symbol) rejected an arithmetic argument whose type differs from
its parameter, although C++ converts it implicitly (a `double` for a `float` parameter, an `int` for an `unsigned`
one). The constructors were corrected in T1; calls kept the same gap.

## Cause

Decision D3 (commit e196da6dd, Items 3.2/3.3) restricted the arithmetic conversions of
`ProjectMethodBodyModel::classifyCFrontierConversion` to functions with C linkage. It protects one behavior:
`pick(1.5)` with a public `pick(int)` and a private `pick(double)` of a C++ class must stay rejected
(pruebas-tanda-10-acceso), because C++ ranks the private `pick(double)` first and then rejects the access, while
NativeSharp only ranks accessible members. Applying C's conversion to C++ calls would have made `pick(int)` viable
and selected it. With that restriction, a C++ call had no arithmetic conversion at all.

## Change

D3 itself is unchanged: `classifyCFrontierConversion` still converts arithmetic values only with C linkage. A
separate C++ rule is added:

- `evaluateNamespaceFunctionCandidateArgument` and `evaluateCallCandidateArgument`
  (`src/Frontend/Bodies/CallAnalyzer.cpp`): for a C++ function or method without C linkage, when no other
  conversion exists, a by-value argument for a by-value parameter, or the value bound to a `const T&` or `T&&`
  parameter, takes `classifyCppArithmeticArgumentConversion` (T1; same set and rank as the C boundary). The
  candidate records `MethodCallCandidate::usesCppArithmeticConversion`.
- The behavior D3 protects is kept explicitly: member lookup now records
  `ExpressionResult::inaccessibleNamedMember` when it skipped an inaccessible member of the called name
  (`MemberAccessAnalyzer.cpp` after `analyzeMethodGroupMember`, `IdentifierAnalyzer.cpp` after
  `analyzeMethodGroupIdentifier`). `evaluateCallCandidates` makes a candidate that needs a C++ arithmetic
  conversion non-viable when such a member exists, so `pick(1.5)` keeps its "no viable overload" result.
  This is conservative: it may reject a call C++ would accept when the inaccessible overload would not have been
  the best one; it never selects an overload C++ would not select.

Code generation: the conversion is part of `selectedConversions`; the call frames of HIR
(`HirCallFrameLowering.cpp`) wrap it in a `Conversion` node before the existing `TemporaryAddress` of a
`const T&`/`T&&` argument, and MIR/LLVM lower it with `convert`/`convertScalar`, as the C boundary's `Numeric`
conversions.

## Preserved behavior

- Calls with C linkage (C functions, `extern "C"` C++ functions, C function pointers): only
  `classifyCFrontierConversion`, unchanged.
- NativeSharp functions and methods: NativeSharp's conversions only.
- `ref`, `in` and `out` arguments, parameter-pack elements and C variadic arguments: unchanged.
- Overload ranking: rank 5, below every NativeSharp implicit conversion; ties between C++ overloads are still
  ordered by T19-006 and stay ambiguous when C++ would.
- A number still reaches a `bool` parameter only through an explicit cast.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0).
- No tests, baselines or probes were run, as requested; the build is not a functional proof. Pending: compiling
  the tutorials that call C++ functions with such arguments, and the access case of pruebas-tanda-10-acceso.
