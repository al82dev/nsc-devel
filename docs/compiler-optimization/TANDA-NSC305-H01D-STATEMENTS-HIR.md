# NSC 3.0.5 — H01D: extracción de statements HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Mover `lowerStatement` y su evaluador privado de `static_assert` a `HirStatementMachine.cpp`. |
| Naturaleza | Movimiento mecánico byte por byte; no se modifica el algoritmo recursivo en H01. |
| Protegido | Orden de lowering, cleanup, regiones, diagnostics, HIR/MIR, sintaxis, semántica, ABI, NSMETA y fixtures. |
| Terminación | Una definición del método y helpers, `nsc` construido y focos HIR 34/34 y 12/12. |

## Mecanismo

La extracción usó marcadores únicos sobre bytes: desde `StaticAssertConstantKind` hasta el helper
siguiente, y desde la firma de `lowerStatement` hasta la firma de `lowerExpression`. El mecanismo
rechaza marcadores ausentes, duplicados, intervalos vacíos o bloques ya presentes en el destino.

No se reindentó ni se reescribió el bloque. `lowerStatement` conserva todas sus llamadas a
`lowerBlock` y `lowerExpression`; H02 transformará la recursión sólo con contrafactual profundo y
paridad exacta.

## Resultado

- `git diff --check`: limpio.
- `nsc` construido; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.

H01D queda cerrado; H01 continúa en H01E.
