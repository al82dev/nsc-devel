# A generic extension member is not a member of a generic owner

NSC Builder run `20261115-233408-dc7151d7`: `TL-LANG-412` (CL-LANG-020), Debug and Release.

## Cause

- Input: `extension Box<T> where T : Special { public int Twice() ... }` and `b.Twice()` on `Box<I>`.
- Observed: "receiver/type 'Box<I>' has no compatible member named 'Twice'"; `--dump-project-generics`
  showed the `Twice` template and no specialization of it.
- `8046d2223` (generic methods on generic owners) treats a template whose parent has generic
  parameters as a member of a generic owner. An extension declaration has the generic parameters of
  its target, so the extension member took that path in three places:
  1. `resolveGenericInferenceCandidates` resolved the receiver type `Box` and returned only templates
     whose parent is `Box`; the extension's parent is the extension, so inference got no candidate.
  2. `processGenericCall` replaced the lexical resolution with the same owner filter and reported
     "has no visible Stage 44 generic method template".
  3. `completeMethodSpecialization` skipped `processNode` for the clone and looked for closed owners,
     which an extension never has, so the specialized body kept `generic:Box`1`.

## Change

- `src/Frontend/Generics/GenericCallInference.cpp`: in (1) and (2), when the receiver type yields no
  candidate, the namespace-level lookup or the lexical resolution stands, as before `8046d2223`.
- `src/Frontend/Generics/SpecializationExecutor.cpp`: in (3), an extension parent is processed as a
  free generic method.

## Validation

- Repro `A:/nsc-fixes-evidencia/b3fix/rest/G412` returns 6.
- `TL-LANG-412` passes in Debug and Release; the full Host.Language, Host.Frontend, Host.Parser and
  Host.Basic components and the `generic_owner_*` integration scripts were run (`b3fix/r2/pyint.txt`).
