# NSC 3.0.5 — H01H1: splits async/generator HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Crear `HirAsyncLowering.cpp` y mover las cuatro transformaciones de funciones async/generator. |
| Naturaleza | Movimiento de un bloque contiguo; ninguna reescritura del algoritmo. |
| Símbolos | `rewriteAsyncPollReturns`, `splitAsyncFunction`, `splitGeneratorFunction`, `splitAsyncGeneratorFunction`. |
| Protegido | Estados, ordinals, frame fields, poll/moveNext/drop, cleanup, mangling y orden de nodos. |
| Fuera de alcance | `AwaitExpression`, conversión iterativa H02 y cualquier cambio de runtime/ABI. |
| Terminación | Definiciones únicas, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron 37,810 bytes contiguos desde `HirBuildCoordinator.cpp`.
- Las cuatro funciones conservan cuerpo, orden y comentarios contractuales.
- `HirAsyncLowering.cpp` quedó registrado explícitamente en `nsc_frontend`.
- No se cambió sintaxis, diagnóstico, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01H permanece abierto hasta H01H2. Esta tanda no afirma validación integral ni ejecuta baseline,
deploy o batería completa.
