# Validación 140 — filtro de estados en Builder 3

La pestaña `Pruebas` incorpora un filtro sobre su tabla principal. Puede
mostrar todas las filas, agrupaciones `Fallos`, `En curso` y
`Pendientes/Listos`, o estados exactos como `PASS`, `FAIL`, `BLOCKED`,
`INFRASTRUCTURE_ERROR` y `KNOWN_FAILURE`.

El filtro es exclusivamente visual: no cambia el plan seleccionado, el
denominador de la corrida, los resultados ni la evidencia persistida. Un
contador indica cuántas filas se muestran respecto al total y las transiciones
en vivo actualizan la visibilidad sin recorrer nuevamente toda la tabla.

Evidencia local: sintaxis Python `PASS`, contratos dirigidos `25/25`, smoke de
GUI `PASS` y `git diff --check` `PASS`. No se compiló ni ejecutó producto, no se
ejecutó baseline y no se tocó `C:`.
