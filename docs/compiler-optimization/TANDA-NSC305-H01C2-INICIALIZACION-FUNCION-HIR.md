# NSC 3.0.5 — H01C2: defaults e inicialización de campos HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Completar `HirFunctionLowerer.cpp` con defaults e inicialización de campos. |
| Naturaleza | Movimiento puro; mismos cuerpos, firmas, diagnósticos, conversiones y orden. |
| Símbolos | `lowerDefaultArgument`, `lowerInstanceFieldInitialization`, `appendInstanceFieldInitializers`, `implicitInitializerTypes`. |
| Protegido | Statements, expresiones generales, calls, aggregates, async, HIR/MIR, ABI, NSMETA y consumidores. |
| Terminación | Una definición por símbolo, `nsc` construido y focos HIR 34/34 y 12/12. |

La autorrecursión de `lowerDefaultArgument` se conserva deliberadamente: H01 sólo separa
responsabilidades físicas. Su eventual migración a frames pertenece a H02.

## Resultado

- `git diff --check`: limpio.
- `nsc` construido; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.

H01C queda cerrado; H01 continúa en H01D.
