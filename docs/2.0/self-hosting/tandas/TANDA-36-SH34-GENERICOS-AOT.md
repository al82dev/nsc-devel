# NativeSharp 2.0 self-hosting — tanda 36: genéricos AOT

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH34` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `606073e62` |
| Commit de cierre | este commit (`selfhost(tanda-36): canonicalize generic AOT instances`) |

## Implementación

`ConsumeGenericCallArguments` ya no descarta el texto que acaba de analizar. Conserva los tipos
normalizados en el callee del `CallExpression`; por ello AST, semántica y fases posteriores comparten
la misma identidad explícita y no necesitan releer tokens.

`GenericInstantiationSet.Build` recorre el árbol completo, produce la clave
`Nombre<Tipos>/Aridad`, elimina duplicados y la vuelca en orden ordinal. La clave no contiene rutas,
direcciones ni orden del filesystem y puede compartirse por metadata entre proyectos. El comando
`--dump-project-generics` convierte esa tabla en una superficie comprobable del compilador.

La fixture declara `Select<T>` con `where T : class`, invoca dos veces `Select<Token>` y ejecuta el
cuerpo escalar concreto. El registro contiene exactamente una instancia en O0–O3 y las ocho rutas
LLD/NSLink devuelven 42.

- [x] argumentos genéricos de call site conservados en AST.
- [x] constraints conservados en plantilla/símbolo.
- [x] identidad AOT canónica, deduplicada y ordenada.
- [x] ejecución O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
