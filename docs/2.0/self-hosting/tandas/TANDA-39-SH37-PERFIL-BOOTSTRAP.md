# NativeSharp 2.0 self-hosting — tanda 39: cierre de superficie bootstrap

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH37` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `8a0591b7f` |
| Commit de cierre | este commit (`selfhost(tanda-39): lower bootstrap compile-time forms`) |

## Implementación y gate

El parser crea `DefaultExpression`; semántica asigna el tipo solicitado y el cero canónico.
`sizeof`/`alignof` ya calculaban layout semántico y ahora el backend materializa sus constantes,
incluidas conversiones escalares. `StaticAssertStatement` atraviesa el backend después del análisis.

La fixture exige las cuatro formas en AST y calcula `30 + sizeof(int) + alignof(long) + default(int)`.
O0–O3 × LLD/NSLink devuelve 42. Las formas de control ya representadas por parser/MIR (`switch`,
`foreach`) permanecen en el contrato acumulativo; EH tiene su gate físico separado en SH38.

- [x] `default(T)`, `sizeof` y `alignof` tipados.
- [x] `static_assert` preservado por el pipeline.
- [x] ejecución O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
