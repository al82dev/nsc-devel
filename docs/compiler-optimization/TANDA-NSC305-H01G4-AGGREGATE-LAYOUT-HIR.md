# NSC 3.0.5 — H01G4: aggregate expressions y layout HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer tuple/aggregate/with y consultas `sizeof`/`alignof`/`offsetof` a `HirAggregateLowering.cpp`. |
| Naturaleza | Movimiento mecánico de dos grupos de casos dentro de un dispatcher privado equivalente. |
| Símbolo | `lowerAggregateOrLayoutExpression`. |
| Protegido | Target types, flattening, offsets, bit-field rejection, constant layout values y ausencia de evaluación en `sizeof(expression)`. |
| Fuera de alcance | Delete/destroy, conversiones, async, excepciones/cleanup e iteratividad H02. |
| Terminación | Casos únicos, H01G completo, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron 4,654 bytes de dos regiones no contiguas.
- Tuple, aggregate initializer y with expression comparten la frontera privada.
- `sizeof`, `alignof` y `offsetof` conservan sus comentarios contractuales y el mismo servicio
  canónico de layout.
- `DeleteExpression`, `DestroyExpression`, conversiones y `AwaitExpression` permanecen en sus
  alcances posteriores; H01G no los absorbió por proximidad física.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01G queda cerrado. Esta tanda no afirma validación integral ni ejecuta baseline, deploy o batería
completa.
