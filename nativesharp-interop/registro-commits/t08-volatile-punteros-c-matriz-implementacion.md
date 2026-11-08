# Tanda 8: punteros C volatile a escalares

Se extiende la ruta directa de una global C puntero a volatile para los tipos escalares admitidos, conservando el modo MS y la alineación correspondiente. LLVM aplica acquire/release a los accesos de byte, 16, 32 y 64 bits, `float` y `double`; se mantiene el filtro de origen C y modo MS.

La construcción incremental y la matriz de `signed char`, `unsigned char`, `unsigned short`, `unsigned int`, `unsigned long long`, `float` y `double` pasaron comparación IR con Clang y ejecución O0–O3: `A:\nsc-interop-build\tanda08-volatile-pointer-19988-21416`. Las regresiones de puntero bool y globales atómicos pasaron O0–O3.

La matriz cubre accesos directos a globales C, no propagación por locales o parámetros, mezclas de opciones de Clang ni todas las formas de puntero. La tanda permanece abierta.
