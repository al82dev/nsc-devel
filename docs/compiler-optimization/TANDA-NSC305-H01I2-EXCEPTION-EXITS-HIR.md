# NSC 3.0.5 — H01I2: exception y structured exits HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer break/continue y return/throw/using/try a `HirExceptionAndCleanup.cpp`. |
| Naturaleza | Movimiento mecánico de dos regiones con dispatcher privado; helper de ordinal centralizado. |
| Símbolos | `cleanupRegionOrdinal`, `lowerExceptionOrExitStatement`. |
| Protegido | Cleanup regions, target scopes, return conversions/borrows, exception targets, catches, filters, finally y orden observable. |
| Fuera de alcance | Coroutine statements, delete/destroy, conversion helpers e iteratividad H02. |
| Terminación | Cuerpos únicos, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron 15,524 bytes de seis casos de statement.
- `cleanupOrdinalFor` conserva su adaptador, respaldado ahora por un método privado único.
- El adaptador local de structured exits se eliminó del statement machine al quedarse sin
  consumidores; async y exception llaman al soporte común.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Incidencia de extracción

El primer script encontró un `default` interno del `switch` de `must_tail` y truncó el bloque. El
diff lo detectó antes de compilar. Los dos archivos afectados se reconstruyeron únicamente desde el
`HEAD` local recién confirmado y la extracción se reaplicó con marcadores anclados al diagnóstico
final del dispatcher. Ese intento no se confirmó ni llegó a commit.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01I permanece abierto para H01I3. Esta tanda no afirma validación integral ni ejecuta baseline,
deploy o batería completa.
