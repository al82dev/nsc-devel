# Inventario de arreglos C con punteros volatile

La prueba de inventario con Clang incluye un arreglo de punteros a volatile,
una matriz con punteros a const volatile y un arreglo de punteros dobles con
calificador anidado. Comprueba conservación de dimensiones y calificadores
en los dos primeros y rechazo del tercero. El presupuesto de la tanda incluye
ahora esta prueba para validarla junto con los casos mixtos.

La prueba está preparada y aún no se ha ejecutado.

Se agregó una declaración equivalente C++ como control negativo para comprobar
que la nueva proyección continúa limitada a C y no habilita esa ruta por
accidente. Este control también espera la validación agrupada.
