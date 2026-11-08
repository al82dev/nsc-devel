# Tanda 8: acceso directo a puntero C de bool volatile

La ruta de HIR identifica una global C `volatile _Bool*` en accesos indexados y por desreferencia, conservando la opción de Clang y la alineación de un byte. LLVM usa acquire en lectura y release en escritura para esa ruta Windows/MS volatile. La escritura por `AddressValue` también utiliza almacenamiento de byte.

La construcción incremental pasó. El control contrastó Clang y NSC y verificó cuatro lecturas acquire y tres escrituras release de byte, además de la ejecución cruzada O0–O3: `A:\nsc-interop-build\tanda08-bool-pointer-18982-20475`. Pasaron la matriz de globales atómicos y la regresión de bool volatile en DLL, ambas O0–O3.

El caso cubre acceso directo a la global en esta configuración. No acredita la conservación de calificadores a través de locales, parámetros, arreglos de punteros u otras opciones de Clang; la tanda sigue abierta.
