# Índice de puntero a C incompleto

Con el compilador anterior, `&value[1]` para `Opaque*` falló en el descenso
porque no se conoce el tamaño del elemento C incompleto. Se preparó el
diagnóstico en la rama de indexación de puntero, sin alterar indexación de
arreglos ni punteros a punteros. El control queda pendiente de validación.
