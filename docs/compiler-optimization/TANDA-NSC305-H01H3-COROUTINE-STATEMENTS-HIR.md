# NSC 3.0.5 — H01H3: coroutine statements HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Completar H01H moviendo `suspend`, `yield return` y `yield break` a `HirAsyncLowering.cpp`. |
| Naturaleza | Movimiento mecánico de casos completos dentro de un dispatcher privado. |
| Símbolo | `lowerCoroutineStatement`. |
| Protegido | Resume/yield state, ref items, cleanup regions, continuation kind y orden de evaluación. |
| Fuera de alcance | Return/throw/using/try, delete/destroy e iteratividad H02. |
| Terminación | Frontera async completa, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- La auditoría posterior a H01H2 detectó tres statements de coroutine aún en
  `HirStatementMachine.cpp`; H01H se reabrió en el tracking en vez de aceptar el hueco.
- Se movieron 3,357 bytes a `lowerCoroutineStatement`.
- `yield break` conserva el mismo adaptador de structured exit, ahora respaldado por
  `applyStructuredExitBinding`.
- No se cambió sintaxis, diagnóstico, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01H queda cerrado con splits, await y statements de coroutine. Esta tanda no afirma validación
integral ni ejecuta baseline, deploy o batería completa.
