# Procedencia HIR del puntero obtenido de arreglo global C

Una desreferencia de `int*` conserva el modo MS cuando su operando es la carga
directa de un elemento de arreglo global C de punteros a volatile. El recorrido
solo acepta índices fijos hasta la dirección del global resuelto, limita su
profundidad y comprueba tipo y calificador. No marca la lectura del slot ni
propaga el modo a locales, parámetros, campos o casts. La validación queda
pendiente de la construcción y la batería agrupadas.
