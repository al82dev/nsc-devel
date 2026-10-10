# Validación 139 — progreso y ejecución exacta por fila

## Alcance

`NSC Builder 3` concentra la ejecución en la pestaña `Pruebas`. La tabla
proyecta una fila por variante exacta `caso|build|optimización`, incluidos los
casos unitarios del ejecutable consolidado.

Cada fila ejecutable muestra progreso, estado, duración viva, última actividad
y una acción `Ejecutar` o `Volver a correr`. Esa misma variante puede lanzarse
desde el menú contextual. Ninguna de las dos acciones amplía el alcance al
componente, contrato, ola o selección completa.

La cabecera conserva el progreso global, la variante activa y los conteos por
estado. `Resultados > Fallos exactos` usa la misma identidad y permite
reejecutar una sola fila fallida.

## Evidencia local

- sintaxis Python: `PASS`;
- contratos dirigidos: `24/24`;
- construcción y cierre de GUI: `PASS`;
- `git diff --check`: `PASS`;
- compilación o ejecución del producto: no realizada;
- validación funcional: pendiente de la ejecución del usuario.

No se ejecutó baseline, no se desplegó y no se tocó `C:`.
