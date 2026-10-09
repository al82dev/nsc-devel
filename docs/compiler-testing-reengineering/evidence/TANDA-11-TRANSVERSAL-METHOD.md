# Evidencia de método — Tanda 11, contratos transversales

## SCOPE LOCK aplicado

Auditoría y consolidación exclusivamente documentales. No se modificó código, pruebas, infraestructura ni
artefactos fuera de `docs/compiler-testing-reengineering/**`; no hubo build ni ejecución.

## Universo

| Elemento | Conteo |
|---|---:|
| Componentes host auditados | 24 |
| Contratos fuente | 870 |
| Casos diseñados conservados | 7,360 |
| Filas de clasificación transversal | 870 |
| Filas de taxonomía de errores | 870 |
| Máquinas de estado | 12 |
| Relaciones contrato-máquina | 515 |
| Contratos con una o más máquinas | 404 |
| Contratos stateless/value | 466 |

## Procedimiento reproducible

1. Se leyó de nuevo `SCOPE-LOCK.md` y `PHASE-1-MASTER-PLAN.md`.
2. Se importó la totalidad de `inventory/contracts.csv`, ordenada por componente e ID.
3. El gate inicial detectó 22 filas históricas de Diagnostics y Lexer sin columna `invariants`, con los campos
   posteriores desplazados. Se reconstruyó esa columna desde los documentos propietarios y se restauró el
   schema sin cambiar IDs, contratos observables ni casos diseñados.
4. Se revisaron los 166 casos diseñados que consumen esas 22 filas: todos conservan contrato, oráculo, documento
   y frecuencia; ninguno heredó el corrimiento del CSV.
5. Cada contrato recibió una fila 1:1 en `transversal-contracts.csv` y `error-taxonomy.csv`.
6. Los seis ejes usan vocabularios cerrados y `NOT_APPLICABLE` explícito.
7. La pertenencia a máquinas usa nombres contractuales y símbolos propietarios, no campos plantilla repetidos.
8. Se revisaron las 515 relaciones y se corrigieron falsos positivos de substrings: `memo/memory`,
   `mode/model`, `process/processing` y `lock/block`.
9. Se recalcularon duplicados, referencias, campos vacíos, IDs, conteos y hashes.

El generador exacto se conserva como `evidence/TANDA-11-CLASSIFICATION-GENERATOR.ps1`. Sólo transforma
inventarios documentales; no lee, construye ni ejecuta producto.

## Huellas

| Archivo | SHA-256 |
|---|---|
| `inventory/contracts.csv` | `0aa9b0e0129e1bf6acab22b5dbfdb6828acfe9ff0533a08802130240c8633055` |
| `inventory/transversal-contracts.csv` | `02a875c63c7e3dbed93ff6f06a67f92fa0eb0271491a9b658abd719a70195e83` |
| `inventory/error-taxonomy.csv` | `db584dbdc3a421a7683eacd9183d5e86390fec842433afa71d3458f70fca0e3e` |
| `inventory/state-machines.csv` | `a0456bea25b080ab37a120bb54724981631df8725937ecd87c923b26f0e29f27` |
| `inventory/state-machine-contract-map.csv` | `9071b28b286da2a42979e4fdf506f4b34f345bce64353072faf4894fc7e5c614` |

## Límites

No se ejecutaron pruebas, compilador, baseline, cobertura ni herramientas de producto. Los conteos demuestran
integridad documental, no comportamiento dinámico.
