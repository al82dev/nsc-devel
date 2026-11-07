# Reproducción de arreglo TLS C

El control C del arreglo `int[2]` pasó lectura, escritura y aislamiento en dos
hilos. NSC `857519a2727c32a2` rechazó el primer caso estático O0 con
`thread-local C/C++ global is not yet projected`. La variante DLL está preparada
y queda pendiente hasta superar el primer caso. La tanda 8 no está cerrada.
