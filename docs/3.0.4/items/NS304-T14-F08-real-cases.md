# NS304-T14-F08 — Casos reales (derive tipado)

- **Estado**: COMPLETO — fixture terminal del derive tipado sin runtime reflection.
- **Rama**: development

## Verificado O0

- `TL-LANG-519` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `record struct Pair
  derive(Equality, Hash, Print, Clone)` con `==`/`Equals`/`GetHashCode`/`ToString`/`Clone` → exit 42.

## Cobertura de los cuatro derives

| derive | miembro generado | sin reflection |
|---|---|---|
| serializer/formatter | `ToString()` ("Pair") | ✅ código tipado |
| binding | `Clone()` (copia de campos) | ✅ sin reflection |
| trait | `Equality` (`==`/`!=`/`Equals`) + `Hash` (`GetHashCode`) | ✅ witnesses tipados |

## Contexto

- El hueco eran los cuatro derives (serializer/formatter/binding/trait) generando código consumible sin
  runtime reflection + la comparación manual vs generado. Los derives tipados (`Equality`, `Hash`,
  `Print`, `Clone`) ya estaban implementados (T02/T14); aquí se cerró la fixture terminal que los
  ejercita juntos y demuestra la comparación manual (campos `X`,`Y`) vs generado (los cuatro miembros).
