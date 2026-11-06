# Calificador volatile del almacenamiento

El análisis transporta `volatile` desde el estado externo hasta sus campos y
elementos de arreglos fijos. Evita tomar una dirección sin preservar el
calificador y no lo propaga a través del valor de un puntero. Este cambio prepara
la semántica; requiere la propagación HIR y validación conjunta.
