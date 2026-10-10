# Validacion 142 - restauracion historica completa en Builder 3

> Correccion de alcance posterior: esta validacion cerro la proyeccion de los
> `5,695` resultados terminales, pero no reconstruyo las `3,892` filas
> no ejecutables de la seleccion original. La restauracion integral de las
> `9,587` filas se cierra en Validacion 143.

## Entrada reproducible

- Ejecucion: `20261010-090333-82a4a13f`.
- Resultados terminales: `5,695`.
- Conteos persistidos: `5,140 PASS`, `247 FAIL`, `88 INFRASTRUCTURE_ERROR`,
  `187 BLOCKED` y `33 KNOWN_FAILURE`.
- Sintoma: al cargar la ejecucion, la tabla podia conservar solamente filas
  visibles con estados anteriores como `LISTO` o `PASS`.

## Causa confirmada

`MainWindow._load_history_path` poblaba primero las `5,695` variantes como
`LISTO` y llamaba `_update_case` una vez por resultado con el ordenamiento y el
filtro de la vista anterior todavía activos. Cambiar el estado podia mover la
fila durante la actualizacion y el filtrado incremental operaba entonces sobre
una posicion que ya no representaba la misma identidad.

El archivo historico no estaba incompleto: `results.json` contiene todos los
estados y sus conteos concuerdan con las filas terminales.

## Cierre implementado

1. La carga historica cambia explicitamente a `Todos los estados`.
2. Ordenamiento y repintado quedan suspendidos durante la proyeccion completa.
3. Los `5,695` resultados se aplican antes de volver a ordenar o filtrar.
4. El conteo restaurado se compara exactamente con el conteo del historial.
5. Una discrepancia ya no produce una vista parcial: aborta con un diagnostico
   que muestra conteos esperados y restaurados.
6. La evidencia original permanece de solo lectura y cualquier reintento sigue
   creando un `run_id` nuevo.

## Alcance de validacion

- Revisión estructural del cambio: completa.
- El cargador consumio la evidencia real y devolvio exactamente `5,695` filas:
  `187 BLOCKED`, `247 FAIL`, `88 INFRASTRUCTURE_ERROR`, `33 KNOWN_FAILURE` y
  `5,140 PASS`.
- Sintaxis Python: correcta.
- Smoke no interactivo de GUI: correcto.
- `git diff --check`: correcto.
- Reapertura y carga grafica de la corrida: pendiente del usuario con la nueva
  revision de `nscbuilder3.py`.
- Compilacion o ejecucion de NSC: no realizada.
- Baseline: no ejecutado.
- Unidad `C:`: no tocada.
