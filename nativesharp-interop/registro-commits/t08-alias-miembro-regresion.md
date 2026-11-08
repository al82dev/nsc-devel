# Regresión al publicar alias de miembros

La construcción incremental de `47ee079f7` pasó y el consumidor con metadata
y sin encabezado pasó O0–O3. Sin embargo, `pruebas-tanda-08-anonimos-c`
falló en O0 con `conflicting C/C++ type alias` cuando el consumidor también
incluía el encabezado. La resolución actual rechaza observaciones de alias
con distinto proyecto dueño; el catálogo de alias elimina la ubicación y el
identificador de referencia actual no demuestra el origen físico común.

Se retira la modificación de producción de `47ee079f7` para no dejar esa
regresión. Los controles y el historial se conservan. Sigue pendiente una
conciliación que demuestre procedencia y contrato antes de publicar esos
alias sin provocar conflictos. Falta reconstruir incrementalmente y repetir
la regresión después de esta reversión; el consumidor sin encabezado vuelve a
estar sin acreditar. La tanda 8 sigue abierta.
