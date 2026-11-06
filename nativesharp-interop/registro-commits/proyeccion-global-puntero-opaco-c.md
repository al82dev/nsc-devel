# Proyección de global C con puntero a tipo incompleto

El fixture mixto rechazó `opaque_current` porque la proyección del global
recorría el puntero y exigía layout completo del struct C apuntado. Se prepara
una ruta para punteros C que comprueba identidad, propietario, origen C y
ausencia de error de importación antes de representar un tipo incompleto. No
se inventa tamaño para el struct ni se habilita su uso por valor. Falta la
construcción incremental y la validación agrupada.
