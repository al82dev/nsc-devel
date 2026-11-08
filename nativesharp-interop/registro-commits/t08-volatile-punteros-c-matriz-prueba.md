# Tanda 8: matriz de punteros C volatile

La matriz incluye punteros globales C a `signed char`, `unsigned char`, `unsigned short`, `unsigned int`, `unsigned long long`, `float` y `double`. Contrasta los anchos, alineaciones y órdenes LLVM con Clang, y prepara ejecución cruzada O0–O3 con lectura y escritura directa e indexada.

La línea base termina al detectar que NSC no emite acquire para los accesos `i8`; el resto de discrepancias no se da por inventariado. Evidencia: `A:\nsc-interop-build\tanda08-volatile-pointer-19606-9036`. La corrección y la validación se registran aparte.
