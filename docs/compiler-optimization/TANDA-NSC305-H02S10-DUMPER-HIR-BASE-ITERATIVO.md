# NSC 3.0.5 — H02S10: dumper HIR base iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de `hir::HirDumper::dumpNode`. |
| Algoritmo | DFS preorden con frames `{node, depth, edge}` y push inverso. |
| Símbolos | `HirDumper::dumpNode` y contrafactual focal en `HirTests.cpp`. |
| Incluido | Orden first/second/third/children, indentación, etiquetas y formato exacto. |
| Excluido | OOP HIR dumper, verifier, builder, lowering, MIR, Driver y Parser. |
| Terminación | Rojo/verde profundo, regresión HIR, build de `nsc`, análisis y tracking. |

## Cadena causal previa al cambio

El dumper HIR base invocaba `dumpNode` una vez por cada arista. Una cadena HIR finita y válida
agotaba el stack del proceso aunque el formato sólo exige preorden, profundidad y la etiqueta de la
arista. El fixture usa ownership plano y un stream contador para aislar el stack sin retener el dump.

## Presupuesto de cambio

- Repositorio: sólo `A:\nsc-devel`.
- Producción: `src/HIR/HirDumper.cpp`.
- Prueba: `tests/unit/HirTests.cpp`; foco `hir-base-dump-depth`.
- Contratos protegidos: texto, orden, indentación, API, ABI y NSMETA.
- Validación: sólo build y pruebas focales O0; sin baseline, batería completa, deploy o instalación.

## Evidencia

- Contrafactual rojo, revisión `e83a5cdf5`: una cadena plana de ownership con 8,192 aristas
  `first` terminó con `0xC00000FD` dentro del dumper recursivo.
- Implementación: `dumpNode` usa una pila explícita de frames `{node, depth, edge}`.
- El push inverso de `children`, seguido de `third`, `second` y `first`, conserva al extraer
  exactamente el orden `first`, `second`, `third` y `children` en orden fuente.
- Foco `NSC_HIR_TEST_FOCUS=hir-base-dump-depth`: `PASS 2/2`.
- Regresión `nsc_hir_tests`: `PASS 34/34`.
- Regresión `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- Build focal `nsc`: completado; `nsc.exe static host-runtime contract: passed`.
- No se ejecutaron baseline, batería completa, deploy ni instalación.

## Complejidad y contratos

El recorrido conserva tiempo lineal respecto del número de nodos más el texto de indentación que ya
formaba parte del contrato. La memoria auxiliar es lineal en el ancho/profundidad pendientes, alojada
en heap en vez del stack del proceso. No cambia el texto emitido, la API, la ABI, NSMETA, los
diagnósticos ni los artefactos del lenguaje.

## Residual H02

Este corte sólo elimina la recursión del dumper HIR base. Permanecen explícitamente pendientes las
rutas recursivas de lowering OOP, `hir::HirBuilder` y `hir::HirVerifier`; no se declara cerrado H02.
