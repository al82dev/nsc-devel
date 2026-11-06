# Controles del puntero C volatile int* directo

El fixture comprueba en IR el store release de valor 61 y una carga acquire
del apuntado a través del global. La prueba unitaria distingue los nodos HIR
marcados de los nodos sin alineación o con otro tipo. Los controles aún no se
han ejecutado; locales, parámetros y arreglos de punteros quedan pendientes.
