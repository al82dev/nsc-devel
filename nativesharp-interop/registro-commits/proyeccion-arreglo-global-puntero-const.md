# Proyección de arreglo global C de punteros a const

El caso `const int *c_slots[2]` se rechazó en `mapGlobalType` porque el
elemento tiene apuntado const. Se prepara la proyección únicamente para C,
arreglo de una dimensión y puntero simple a escalar; el calificador del
apuntado se conserva en el estado semántico del global. Las demás variantes
calificadas siguen rechazadas. Falta transportar esa marca al resultado de la
indexación y validar el conjunto antes de afirmar compatibilidad.
