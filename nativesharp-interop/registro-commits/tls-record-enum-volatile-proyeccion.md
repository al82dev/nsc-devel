# Proyección de records y enums TLS volatile

El control C pasó y NSC rechazó los cuatro globales en la barrera TLS. Se
permite que record y enum `volatile`, incluidos arreglos, lleguen al mapeo C
existente, que valida identidad y layout y conserva calificadores. Los tipos no
mapeados siguen rechazados. Pendiente construcción incremental y pruebas
agrupadas estática y DLL; no se afirma cobertura general de agregados.
