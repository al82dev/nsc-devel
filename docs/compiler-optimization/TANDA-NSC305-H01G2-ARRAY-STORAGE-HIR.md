# NSC 3.0.5 — H01G2: construcción array/storage HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer `StackallocExpression`, `NewExpression` y `CollectionExpression` a `HirAggregateLowering.cpp`. |
| Naturaleza | Movimiento físico de casos completos dentro de un dispatcher privado equivalente. |
| Símbolo | `lowerArrayConstructionExpression`. |
| Protegido | Orden de evaluación, tamaños/layouts, conversions, spreads, builder ordinals, inicializadores y diagnostics. |
| Fuera de alcance | Object/placement construction, aggregate expressions, consultas de layout e iteratividad H02. |
| Terminación | Casos únicos, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron 15,219 bytes desde `lowerExpressionImpl`.
- El dispatcher principal conserva tres etiquetas y delega a una sola frontera privada.
- El nuevo dispatcher sólo acepta esas tres clases de nodo; su `default` es un guardia interno
  inalcanzable desde la ruta pública.
- Los cuerpos de `stackalloc`, collection builder y array allocation no fueron reescritos.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01G permanece abierto para H01G3 y H01G4. Esta tanda no afirma validación integral ni ejecuta
baseline, deploy o batería completa.
