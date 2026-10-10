# Validacion 145 — Historial por corrida en Builder 3

## Objetivo

Separar completamente la ejecucion viva de la consulta de corridas guardadas.

## Implementacion

- `Pruebas` inicia y permanece como espacio de una corrida nueva.
- `Limpiar resultados para nueva corrida` reinicia exclusivamente el estado
  vivo; no borra carpetas ni tabs historicos.
- `Resultados > Historial` abre una pestaña independiente por corrida.
- Abrir nuevamente la misma corrida enfoca su pestaña existente.
- Cada pestaña restaura las filas terminales y no ejecutables, estado, hora,
  tiempo desde inicio, duracion numerica, detalle y ruta de evidencia.
- Los filtros historicos cubren estado, componente, momento de ejecucion,
  duracion y busqueda textual.
- Cada corrida expone tabla, log completo, reporte condensado, resumen
  guardado, eventos, `results.json` y metadatos.
- Cerrar una pestaña no elimina evidencia; reejecutar una fila produce una
  corrida nueva en el espacio vivo.

## Aseguramiento

- `python -m py_compile nscbuilder3.py`: PASS.
- `git diff --check -- nscbuilder3.py`: PASS.
- `QT_QPA_PLATFORM=offscreen python nscbuilder3.py --gui-smoke`: PASS.
- El smoke cargo una corrida real guardada y verifico que tabla, conteos, log y
  reporte del espacio vivo permanecieran identicos.
- No se compilo ni ejecuto producto, no se ejecuto baseline, no se desplego y
  no se toco `C:`.

## Estado

Implementacion lista para inspeccion visual del usuario. La tarea persistente
continua con la clasificacion causal de los errores de infraestructura de la
corrida funcional.
