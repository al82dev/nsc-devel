# Tanda 8: arreglos de punteros C volatile a escalares

La ruta de HIR para un puntero leído desde un arreglo global C verifica el tipo del elemento y conserva el modo MS para los escalares admitidos, en vez de limitarse a `int*`. La condición mantiene la comprobación de origen C, calificador y tipo; no se aplica a punteros arbitrarios ni a locales.

Tras construcción incremental, la matriz de arreglos de punteros a `_Bool`, enteros de 8, 16, 32 y 64 bits, `float` y `double` pasó comparación de IR contra Clang y ejecución O0–O3: `A:\nsc-interop-build\tanda08-volatile-pointer-20686-27815`. Pasaron además las regresiones de punteros bool y globales atómicos en O0–O3.

Se cubre una dimensión en esta matriz, con acceso tanto indexado como desreferenciado. El transporte por locales, parámetros, otras opciones volatile y formas adicionales siguen pendientes; no se cierra la tanda.
