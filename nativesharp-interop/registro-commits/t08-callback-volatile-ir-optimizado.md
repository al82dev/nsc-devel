# T08: IR optimizado de callbacks C volatiles

El control de callbacks comparaba el IR sin optimizacion, aunque ejecutaba el programa de O0 a O3. Ahora compara tambien el IR generado en O1, O2 y O3, comprobando accesos simples para la volatilidad heredada y acquire/release para campos y globales declarados volatile, ademas del caso empaquetado.

`pruebas-tanda-08-callback/validar.cmd` termino con codigo 0. El control de Clang de referencia se genera en O0; no representa una comparacion completa de todos los layouts o rutas precompiladas. La tanda 8 sigue abierta.
