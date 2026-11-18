# The compiler-synthesized rest view must not depend on `using NativeSharp.Core`

NSC Builder run `20261115-233408-dc7151d7`: `TL-LANG-357`, `TL-LANG-358`, `TL-LANG-364`, `TL-LANG-366`
(CL-LANG-020) and `TF-FRONT-167` (CF-FRONT-021), Debug and Release.

## Cause

- Input: `match(values) { [10, .. var middle, 40] => ... }` over `int[]` or `int[4]`, with the
  NativeSharp.Core sources in the project (the TL-LANG cases stage them from
  `A:/nativesharp-libraries/stdlib/NativeSharp.Core`) and no `using NativeSharp.Core`.
- Observed: "array/inline rest capture requires a materialized Span<T>/ReadOnlySpan<T>".
- The pre-Sema pass (`collectBodyTypeSpecializationDemand`) requests `Span<int>` and
  `ReadOnlySpan<int>`, and `processBodyTypeDemandWave` resolves the template with
  `resolveTypeTemplate`. Since `a79b0d26f` that function resolves only through the lexical ladder of
  the call site, so a `Span<T>` in a namespace the source does not import is missed and no
  specialization is materialized. Sema then names the view by template name
  (`closedTypeKeyForTemplate`) and finds nothing.
- The same source with `using NativeSharp.Core;` compiled and returned 52; the same failure happened
  with the Core files from before their removal (`8c8f1704f^`), so the library sources are not the
  cause.

## Change

`src/Frontend/Generics/TypeDemandScheduler.cpp`, `processBodyTypeDemandWave`: when both lexical
resolutions miss, the demand takes the first visible template with that name and arity, the one Sema
looks up. Only rest-capture demands use this wave; ordinary type names keep the lexical ladder.

## Validation

- Repro `A:/nsc-fixes-evidencia/b3fix/rest/T03ListArray` compiles and returns 52.
- `nsc_component_tests` Debug and Release: the five cases pass; Host.Parser, Host.Frontend,
  Host.Language and Host.Basic were run in full in both modes (`A:/nsc-fixes-evidencia/b3fix/comp4.txt`).
