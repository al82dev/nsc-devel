# NSC 3.0.5 — H01H2: await HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer `AwaitExpression` a `HirAsyncLowering.cpp`. |
| Naturaleza | Movimiento mecánico del cuerpo; ninguna reescritura del algoritmo. |
| Símbolo | `lowerAwaitExpression`. |
| Protegido | Direct poll, future-variable poll, detección transitiva de suspensión, resume state, rethrow, cleanup y exception target. |
| Deuda visible | Las lambdas recursivas `canSuspend`/`scan` se preservan en H01; su sustitución iterativa pertenece a H02. |
| Terminación | Caso único, H01H completo, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron 94 líneas, equivalentes a 6,129 bytes.
- El dispatcher principal conserva `AwaitExpression` y delega a `lowerAwaitExpression`.
- La detección recursiva de suspensión no se ocultó ni se declaró resuelta: H02 deberá reemplazarla
  con worklists explícitas y paridad exacta.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01H queda cerrado. Esta tanda no afirma validación integral ni ejecuta baseline, deploy o batería
completa.
