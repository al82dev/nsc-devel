# Tanda de validación 155 — evidencia de oráculos y auditoría residual

## SCOPE LOCK

- **Objetivo:** cerrar la evidencia perdida por los oráculos `ProjectCompilation` y auditar la integridad de todos los `FAIL` de la corrida `20261010-090333-82a4a13f`.
- **Protegido:** código de producto NSC, contratos, expectativas, suite histórica, unidad `C:` y los seis archivos preexistentes registrados en `CURSOR.json`.
- **Prohibido:** adaptar el compilador para hacer pasar pruebas, convertir fallos en éxitos, retirar casos o ejecutar validaciones parciales antes de cerrar las cinco correcciones.
- **Validación funcional autorizada:** una sola construcción y corrida integral `Debug/O0/Todos` al terminar esta tanda.

## Auditoría reproducible de los `247 FAIL`

La corrida guardada se cruzó contra `manifest/cases.jsonl`, `contracts.csv` y los documentos de contrato. El resultado mecánico fue:

| Comprobación | Resultado |
|---|---:|
| Filas `FAIL` | 247 |
| Casos únicos | 247 |
| Contratos afectados | 95 |
| Casos ausentes del manifiesto | 0 |
| Resultados esperados distintos de `PASS` | 0 |
| Contratos ausentes | 0 |
| Contratos sin símbolos, oráculo, documento o sección | 0 |
| Documentos de contrato ausentes | 0 |
| Fallos sin detalle | 0 |

La distribución completa coincide con el análisis causal previo:

| Componente | Contratos | Fallos |
|---|---:|---:|
| Host.Compilation | 4 | 11 |
| Host.Dominance | 1 | 1 |
| Host.Driver | 5 | 18 |
| Host.Frontend | 39 | 106 |
| Host.HIR | 4 | 5 |
| Host.Interop | 2 | 16 |
| Host.LanguageService | 5 | 14 |
| Host.MIR | 10 | 16 |
| Host.Parser | 5 | 8 |
| Host.Passes | 2 | 14 |
| Host.Sema | 14 | 33 |
| Host.SSA | 5 | 5 |
| **Total** | **95** | **247** |

Esta comprobación demuestra que el catálogo no perdió ni reclasificó fallos. No demuestra que los 247 sean defectos del producto; esa decisión exige el valor observado de una ejecución nueva.

## Defecto confirmado del oráculo compartido

Sesenta y cinco fallos de trece contratos Frontend cruzaban el mismo helper `expectProjectStage()`. El helper recibía `loadError` y conservaba `DiagnosticEngine`, pero publicaba únicamente el texto genérico `valid project fixture must reach its requested frontend stage`. La corrida no permitía distinguir carga de proyecto, sintaxis, semántica ni etapa incompleta.

La corrección:

- conserva el error de carga exacto;
- conserva los contadores de errores y diagnósticos;
- renderiza todos los diagnósticos mediante `DiagnosticRenderer` y el `SourceManager` propietario;
- añade la misma evidencia al camino histórico de `FrontendCompilation`;
- mantiene intacto el valor booleano del oráculo y todas sus expectativas;
- materializa temporales bajo el directorio de trabajo del caso, no en la unidad temporal global.

Los cinco tests del materializador self-host también colocan sus temporales bajo `A:\nsc-devel\_nscbuilder3\test-temp`; no escriben en `C:`.

## Frontera de aceptación

Después de este cambio, un `FAIL` continúa siendo `FAIL`; sólo deja de ser opaco. No se modificó ningún contrato para hacerlo pasar. La única autoridad funcional será la corrida completa final. Cualquier `INFRASTRUCTURE_ERROR` residual o contradicción del sobre seguirá siendo defecto de infraestructura y no se presentará como problema del compilador.

## Estado

- Corrección implementada: sí.
- Auditoría estática: `247/247` filas y `95/95` contratos contabilizados.
- Producto modificado: no.
- Expectativas modificadas: no.
- Pruebas retiradas: no.
- Validación integral: pendiente hasta completar este commit; se ejecutará una sola vez en `Debug/O0/Todos`.
