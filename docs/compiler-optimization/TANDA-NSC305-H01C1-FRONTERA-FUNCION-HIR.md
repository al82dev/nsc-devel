# NSC 3.0.5 — H01C1: frontera básica de función HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Mover bloque, receiver y fuente de deconstruction a `HirFunctionLowerer.cpp`. |
| Naturaleza | Movimiento puro; cero cambios de cuerpos, firmas, orden o representación HIR. |
| Símbolos | `lowerBlock`, `lowerReceiver`, `lowerDeconstructionSource`. |
| Protegido | Statements, dispatcher de expresiones, calls, conversions, async, aggregates, HIR/MIR, ABI, NSMETA y diagnósticos. |
| Terminación | Una definición por símbolo, `nsc` construido y focos HIR 34/34 y 12/12. |

Este corte establece la unidad de función sin convertir recorridos a frames. La migración iterativa
permanece reservada para H02.

## Resultado

- `git diff --check`: limpio.
- Una definición por cada símbolo movido.
- `nsc` construido; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.

H01C permanece abierto por H01C2.
