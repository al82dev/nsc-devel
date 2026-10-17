# NSC 3.0.5 — H01I1: soporte exception/cleanup HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Crear `HirExceptionAndCleanup.cpp` y extraer soporte compartido de exits y cleanup de expresión completa. |
| Naturaleza | Movimiento de cuerpos con adaptador local estable; ninguna reescritura de semántica. |
| Símbolos | `applyStructuredExitBinding`, `wrapFullExpressionCleanup`. |
| Protegido | Cleanup region ordinals, target scopes, continuation kinds, owned-temporary lifetime y orden observable. |
| Fuera de alcance | Statements suspend/yield, return/throw/using/try, delete/destroy e iteratividad H02. |
| Terminación | Definiciones únicas, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- La lambda local que aplicaba `MethodStructuredExitBinding` delega ahora en un método privado
  único con el mismo `statement` y el mismo nodo destino.
- `wrapFullExpressionCleanup` salió del coordinador sin cambiar su cuerpo.
- `HirExceptionAndCleanup.cpp` quedó registrado explícitamente en `nsc_frontend`.
- La auditoría detectó que suspend/yield seguían en el statement machine; H01H se reabrió con H01H3
  en vez de ocultar esa frontera incompleta.
- No se cambió sintaxis, diagnóstico, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01I permanece abierto para H01I2/H01I3. Esta tanda no afirma validación integral ni ejecuta
baseline, deploy o batería completa.
