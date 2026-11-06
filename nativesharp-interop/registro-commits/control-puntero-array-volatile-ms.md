# Controles de orden del apuntado desde arreglo C

El fixture comprueba escrituras release de valores 97, 98 y 99 mediante
punteros obtenidos de arreglo, matriz y arreglo de punteros constantes, más
una lectura acquire. Las comprobaciones no afirman que el slot del puntero
sea volatile. Los resultados esperan la siguiente validación agrupada.

También se comprueba que las lecturas y escrituras del propio slot no se
marquen como `atomic volatile` en este fixture, donde solo el apuntado lo es.
