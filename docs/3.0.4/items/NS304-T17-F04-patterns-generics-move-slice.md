# NS304-T17-F04 — Patterns + generics + move-only + borrowed slices

- **Estado**: COMPLETO — la combinación de las cuatro aristas verificada.
- **Rama**: development

## Verificado O0

- `TL-LANG-515` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `First<T>(ReadOnlySpan<T>)`
  + `Make(v) { owned Cell c; return move(c); }` + `match (moved.Value)` → exit 42.

## Cobertura de las cuatro aristas

| arista | evidencia |
|---|---|
| patterns | `match (moved.Value) { 42 => 42, _ => 0 }` |
| generics | `First<T>(ReadOnlySpan<T>)` + `First<int>(values)` |
| move-only | `owned Cell c` + `return move(c)` (consuming context) |
| borrowed slices | `ReadOnlySpan<T>` sobre `int[]` |

## Contexto

- El hueco era probar la combinación (no una feature aislada). Las cuatro aristas ya tenían pruebas
  individuales; aquí se cerró la combinación productiva end-to-end. El `move(...)` sólo es válido en
  contexto consumidor (`return`, no asignación arbitraria), consistente con el contrato de ownership.
