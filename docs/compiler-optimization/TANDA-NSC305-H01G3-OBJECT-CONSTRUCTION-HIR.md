# NSC 3.0.5 — H01G3: object/placement construction HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer `ObjectCreationExpression` y `PlacementConstructionExpression` a `HirAggregateLowering.cpp`. |
| Naturaleza | Movimiento mecánico del cuerpo compartido; ninguna reescritura del algoritmo. |
| Símbolo | `lowerObjectConstructionExpression`. |
| Protegido | Allocator provenance, placement, failure profiles, ctor selection, packs, defaults, borrows, cleanup, field initializers y orden de evaluación. |
| Fuera de alcance | Aggregate expressions, consultas de layout, destrucción/cleanup e iteratividad H02. |
| Terminación | Dispatcher único, cuerpo equivalente, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron 473 líneas, equivalentes a 27,529 bytes del cuerpo compartido.
- El dispatcher principal conserva ambas etiquetas y delega a
  `lowerObjectConstructionExpression`.
- La ruta ordinaria, la ruta placement y las rutas de allocator/failure permanecen en el mismo
  orden textual y con los mismos diagnostics.
- No se cambió sintaxis, diagnóstico público, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01G permanece abierto para H01G4. Esta tanda no afirma validación integral ni ejecuta baseline,
deploy o batería completa.
