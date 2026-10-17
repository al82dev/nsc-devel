# NSC 3.0.5 — H01I3: destruction HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer `DeleteExpression` y `DestroyExpression` a `HirExceptionAndCleanup.cpp`. |
| Naturaleza | Movimiento mecánico de casos completos dentro de un dispatcher privado. |
| Símbolo | `lowerDestructionExpression`. |
| Protegido | Array destruction, element type, destructor member, deallocation y orden de evaluación. |
| Fuera de alcance | Conversiones/borrows e iteratividad H02. |
| Terminación | Casos únicos, H01I completo, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron 6,839 bytes desde `lowerExpressionImpl`.
- El dispatcher principal conserva `delete`/`destroy` y delega a una sola frontera privada.
- No se absorbieron casts ni otras expresiones contiguas físicamente.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01I queda cerrado. Esta tanda no afirma validación integral ni ejecuta baseline, deploy o batería
completa.
