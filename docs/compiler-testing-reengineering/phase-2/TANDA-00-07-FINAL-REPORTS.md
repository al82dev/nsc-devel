# Tanda 00.07 — Reportes finales completos

## Resultado

`ReportWriter` produce tres artefactos deterministas bajo la corrida en `A:`:

- `results.json`: metadata, identidades de herramientas, conteos y resultados completos;
- `summary.md`: resumen humano por estado;
- `failures.log`: evidencia íntegra sólo de fallos, bloqueos, cancelaciones, skips y errores de infraestructura.

`events.jsonl` pertenece al writer progresivo de la tanda anterior y sobrevive aunque el reporte final no alcance a
cerrarse.

## Gates

- la selección no está vacía;
- no existen IDs seleccionados o resultados duplicados;
- cada seleccionado tiene exactamente un resultado;
- no hay resultados ajenos a la selección;
- ningún archivo final existente se sobrescribe;
- los temporales se sincronizan antes de su publicación atómica;
- el destino pertenece a `A:`.

El reporte conserva revisión, scope, perfil, build mode, optimización y herramientas efectivamente utilizadas con
ruta y SHA-256. No mezcla salida de build ni infiere herramientas alternativas.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_report.py` valida el conjunto completo, orden estable, cobertura 1:1,
preservación de evidencia anterior y frontera de unidad.
