# Validación inicial de miembros anónimos C en bibliotecas

El fixture `pruebas-tanda-08-anonimos-c/validar.cmd` terminó con código 0 usando
`A:/nsc-interop-build/tanda1-ON` y Clang 23 de `A:/al82dev-clang`. En cada nivel
O0, O1, O2 y O3 compiló y ejecutó el caso directo, una biblioteca consumida junto
con el encabezado y la misma biblioteca consumida solo con metadata.

El caso contrasta el tamaño de `LAnonymous` y la escritura de sus campos promovidos
desde NativeSharp con la lectura realizada por C. Incluye una union y un struct
anónimos como miembros del record propietario.

La prueba no cubre todos los anidamientos, empaquetados, bitfields, conflictos de
nombres ni compatibilidad de catálogos históricos. Tampoco es la batería completa
de la tanda 8; T08-010 y T08-011 siguen abiertos.
