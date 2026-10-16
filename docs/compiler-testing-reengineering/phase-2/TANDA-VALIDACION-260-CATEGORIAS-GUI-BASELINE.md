# Validación 260 — categorías visibles en la GUI del baseline

## Estado

`MEJORA DE PRESENTACIÓN IMPLEMENTADA; SMOKE GUI Y SUITE MODULAR COMPLETOS; MATRIZ NO EJECUTADA`.

## Problema observado

La pestaña `Baseline` sólo presentaba la etapa activa, una barra de progreso y el log completo. El catálogo ya
distinguía la capa canónica de cada caso, pero la certificación no mostraba si una fila pertenecía a pruebas
unitarias o externas ni exponía el resultado individual en su propia superficie.

La ejecución ya emitía resultados por variante mediante `case_ready`, pero la pestaña no conectaba esa señal.

## Corrección

- Se añadió una proyección explícita y comprobable desde `CaseRecord.layer`:
  - `unit` -> `Prueba unitaria`;
  - cualquier otra capa válida del catálogo -> `Prueba externa`.
- La previsualización del baseline carga nuevamente el catálogo canónico y expande exactamente la matriz que se
  certificará.
- La pestaña muestra conteos separados de casos y variantes por categoría.
- Se añadió una tabla con las columnas:
  `Categoría`, `Caso`, `Componente`, `Build`, `Opt`, `Estado`, `Duración` y `Detalle / evidencia`.
- La señal `case_ready` actualiza en vivo estado, duración y evidencia de la variante exacta.
- Al terminar, la tabla se reconstruye desde los resultados finales sin cambiar ejecución, selección ni
  oráculos.
- La actualización desactiva temporalmente el sorting para impedir que el movimiento de una fila haga que
  duración o evidencia se escriban en otra variante.

No se crearon casos, no se modificó el manifiesto y no se dedujo la categoría mediante prefijos o nombres.

## Cobertura observada

El catálogo vivo produjo estos conteos:

| Categoría | Casos canónicos | Variantes del baseline |
|---|---:|---:|
| Prueba unitaria | 3,149 | 7,246 |
| Prueba externa | 2,842 | 5,888 |
| Total | 5,991 | 13,134 |

Las sumas coinciden exactamente con el inventario y la matriz vigentes; no quedan casos ni variantes sin
categoría.

## Validación ejecutada

- pruebas focales de operaciones y clasificación del baseline: `10/10`, `OK`;
- suite modular completa del Builder: `73/73`, `OK`;
- smoke de GUI: código de salida `0`;
- auditoría del catálogo: `5,991` casos, `682` contratos, `30` componentes y `5,991` ejecutables;
- smoke estructural de la tabla: ocho encabezados, categoría canónica y actualización en vivo de `PASS`, duración
  y evidencia.

## Límites

- No se ejecutó la matriz completa del baseline.
- No se configuró ni compiló NSC para esta mejora visual.
- No se instaló ni promovió ningún artefacto.
- La nueva tabla informa la misma ejecución existente; no constituye un segundo runner.

