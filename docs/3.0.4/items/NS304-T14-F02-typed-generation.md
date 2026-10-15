# NS304-T14-F02 — Generación tipada

- **Estado**: COMPLETO — builders AST validados (origin + hygiene + stable ID) + gate de colisión del mismo binder.
- **Rama**: development

## Implementado (2026-10-15)

- `63f4d53f` — cierre del hueco auditado:
  - **Builders AST validados**: `MetaprogrammingAstBuilder` (`include/nativesharp/Language/MetaprogrammingGeneration.h`,
    implementado en `MetaprogrammingSemantics.cpp`) construye declaraciones generadas tipadas —
    `buildField`, `buildMethod`, `buildImpl`, `buildHelper` — cada una con `GeneratedDeclarationKind`,
    `HygienicName` (spelling + scope = stable ID), `typeKey`, `originPath` (source origin) y
    modificadores (`isPublic`/`isStatic`/`isReadonly`). La validación rechaza nombre vacío, el
    namespace reservado `__nsc_`, origen vacío y tipo vacío con un `error` causal.
  - **Mismo binder**: `wouldCollide(existing, declaration)` aplica `sameBinding` para detectar una
    colisión con un miembro existente del mismo scope antes del binding.

## Verificado O0

- `TL-LANG-500` (CL-LANG-014, Host.Language, `nsc_component_tests`): PASS — field/method/impl/helper
  llevan origin + hygiene scope + stable ID; rechazo de `__nsc_` y nombre vacío; `wouldCollide` sólo
  colisiona con spelling idéntico en el mismo scope.
- Unit `nsc_ns3_t11_metaprogramming_tests`: PASS (14 checks, incluye las mismas comprobaciones).

## Contexto

- El hueco era la mitad "generación" del vertical tipado (la otra mitad, la introspección, la cerró
  T14-F01): los `builders AST validados` con origin + hygiene context + stable ID y el gate del
  mismo binder. La semántica de los derives concretos (bodies de `Equals`/`Clone`, etc.) pertenece a
  T12-F06/T14-F05 y no se sustituyó por ABI inventado.
