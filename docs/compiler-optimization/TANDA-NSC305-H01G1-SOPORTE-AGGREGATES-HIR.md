# NSC 3.0.5 — H01G1: soporte de deconstruction y aggregates HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Crear `HirAggregateLowering.cpp` y mover soporte compartido de deconstruction y aggregates. |
| Naturaleza | Movimiento físico por marcadores únicos; ninguna modificación de cuerpos. |
| Símbolos | `lowerDeconstructionSource`, `appendAggregateElements`, `appendAggregateLeaves`. |
| Protegido | Selección de deconstructor, layouts, offsets, bit fields, spreads, conversions, ownership y orden de evaluación. |
| Fuera de alcance | Casos de expresión de construcción/aggregate/layout; conversión iterativa H02. |
| Terminación | Definiciones únicas, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- `lowerDeconstructionSource` salió de `HirFunctionLowerer.cpp`.
- `appendAggregateElements` y `appendAggregateLeaves` salieron de
  `HirBuildCoordinator.cpp`.
- Los tres cuerpos, incluyendo comentarios contractuales, suman 16,454 bytes movidos sin
  reindentación ni reescritura.
- `HirAggregateLowering.cpp` quedó registrado explícitamente en `nsc_frontend`.
- No se cambió sintaxis, diagnóstico, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01G permanece abierto hasta mover los casos de array/storage, object/placement y
aggregate/layout. Esta tanda no afirma validación integral ni ejecuta baseline, deploy o batería
completa.
