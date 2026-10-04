# NS3-T06-F03 — Reborrow and split borrows

## Resultado

NativeSharp 3.0 trata cada local `ref T` como un préstamo mutable exclusivo de un lugar de
storage. El frontend conserva una ruta canónica, permite reborrow léxico, suspende el alias padre
mientras vive el hijo y demuestra disjunción para campos y elementos de arrays con índices
constantes. Cuando el layout, el índice o el protocolo no prueban disjunción, rechaza de forma
conservadora.

## Cambios

- La edición del proyecto llega al modelo de cuerpos; la nueva exclusividad solo aplica a
  `Language=3`. Edición 2 conserva su comportamiento histórico de referencias.
- Cada local ref conserva `borrow-place` y, si procede, `reborrow-source-local`.
- El place graph usa roots `L<n>`/`P<n>`, fields `F:<name>`, índices `I:<valor>` y wildcard
  `I:*`; también representa slices constantes half-open `S:<inicio>:<fin>`.
- Un reborrow hereda el lugar del alias fuente. El hijo puede usarlo; el padre queda suspendido
  hasta salir del scope del hijo.
- Fields hermanos e índices constantes distintos de arrays/fixed arrays/punteros se admiten en
  paralelo. Un owner completo, un ancestro, la misma ruta o un índice dinámico se solapan.
- Indexers definidos por usuario, unions y agregados con offsets explícitos usan wildcard:
  distintas grafías no prueban storage distinto.
- El chequeo de move parcial consume la ruta completa; mover un field hermano ya no queda
  bloqueado por un ref de otra proyección, pero mover el owner completo continúa prohibido.
- Se restauró en el parser el diagnóstico de `ref var` escrito por el usuario. Los temporales
  `ref var` generados internamente por pattern lowering no pasan por esa ruta fuente.

## Archivos nuevos

- `tests/unit/NativeSharp3T06ReborrowSplitTests.cpp`: fixture dirigida O0/NSLink con ejecución,
  fields hermanos, arrays, calls, loop, reborrow, suspensión, aliasing, fallback dinámico,
  indexer conservador, whole move y compatibilidad de edición 2.
- `docs/3.0/language/reborrow-split-borrows.md`: contrato normativo bilingüe.
- Este archivo: evidencia, decisiones y mapa del commit.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`.
- `src/Frontend/ProjectCompilation.cpp`.
- `src/Frontend/ProjectMethodBodyModel.cpp`.
- `src/Frontend/ProjectMethodBodyModelDumper.cpp`.
- `src/Parser/Parser.cpp`.
- `CMakeLists.txt`.
- `docs/3.0/TANDA-06-OWNERSHIP-PRESTAMOS-Y-DESTRUCCION.md`.
- `mkdown/NativeSharp-Language-3.0-Roadmap.md`.

## Mejora observable

Un algoritmo puede trabajar simultáneamente con `ref pair.Left` y `ref pair.Right`, o con
`ref values[0]` y `ref values[1]`, sin desactivar la seguridad del owner completo. No se añade
storage, flag, thunk ni cambio ABI: las rutas existen solo en el frontend y en el dump. Los casos
ambiguos siguen fallando cerrados.

## Validación dirigida

- `nsc.ns3.t06.reborrow-split`: 11/11 checks, incluida ejecución real O0 mediante NSLink.
- Matriz relacionada: F01, F02, ownership base, array borrow, reference locals, reference returns,
  reference indexers y reference lifetime analysis.
- Construcción incremental del target dirigido y del ejecutable `nsc`: requerida antes del commit.
- Baseline: no ejecutado por prohibición explícita del proyecto.
