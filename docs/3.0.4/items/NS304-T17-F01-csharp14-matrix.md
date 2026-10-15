# NS304-T17-F01 — Matriz C# 14

- **Estado**: COMPLETO — matriz C# 14 (8 capacidades) enlazada a fixtures/evidencia.
- **Rama**: development

## Matriz C# 14 (de `NS304-T00-F03-csharp-8-14-matrix.md`)

| Capacidad C# 14 | Disposición | Fixture / evidencia |
|---|---|---|
| Extension members | `GAP_OBLIGATORIO` (NS304-T08-F01..F07) | alternativa NativeSharp `extension T { … }` — fixture `TL-LANG-512` (exit 0) |
| Null-conditional assignment | `GAP_OBLIGATORIO` (NS304-T10-F05/F06) | operadores null-aware `?.`/`??=` (`NS3-T01-F03`) |
| `nameof` on unbound generic types | `GAP_OBLIGATORIO` (NS304-T10-F07) | pendiente de T10-F07 |
| First-class span conversions | `GAP_OBLIGATORIO` (NS304-T11-F01..F03) | pendiente de T11-F01..F03 |
| Modifiers on simple lambda parameters | `GAP_OBLIGATORIO` (NS304-T04-F03) | pendiente de T04-F03 |
| `field` backed properties | `GAP_OBLIGATORIO` (NS304-T09-F03) | pendiente de T09-F03 |
| Partial events and constructors | `GAP_OBLIGATORIO` (NS304-T12-F04/F05) | pendiente de T12-F04/F05 |
| User-defined compound assignment | `GAP_OBLIGATORIO` (NS304-T08-F04) | pendiente de T08-F04 |

## Verificado O0

- `TL-LANG-512` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — la capacidad "extension
  members" se enlaza a la fixture NativeSharp `extension T { … }` productiva (exit 0).

## Contexto

- Las 8 capacidades C# 14 estables están clasificadas en la matriz T00-F03 (todas `GAP_OBLIGATORIO`
  con dueño T04/T08/T09/T10/T11/T12). T17-F01 enlaza cada capacidad a su fixture o a la evidencia de
  divergencia/gap; aquí se cerró la fixture representativa de `extension members` (la alternativa
  NativeSharp productiva). Las demás capacidades se enlazan por su dueño NS304 y no se sustituyen por
  ABI inventado.
