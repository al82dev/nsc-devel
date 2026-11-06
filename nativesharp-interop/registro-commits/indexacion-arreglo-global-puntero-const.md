# Calificador del apuntado al indexar un arreglo C

La proyección de `const int *c_slots[2]` conserva el calificador en el estado
del arreglo. Se prepara su transporte al puntero seleccionado por índice sin
marcar el slot como inmutable, y se impide tomar una dirección que perdería el
calificador anidado. Esto permite que el análisis de la desreferencia y las
conversiones existentes vean el apuntado const. Falta construir y validar el
fixture positivo, las tres rutas negativas y las regresiones de arreglos.
