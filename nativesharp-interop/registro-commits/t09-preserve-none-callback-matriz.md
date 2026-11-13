# Matriz de callbacks escalares C preserve_none

La llamada mediante puntero `preserve_none` admite las cuatro formas comprobadas: `float(float)`, `unsigned int(unsigned int)`, `unsigned long long(unsigned long long)` y `double(double)`. Se mantiene la convención en el tipo, la llamada y el LLVM emitido; la restricción explícita sigue rechazando otras firmas indirectas que todavía no están implementadas.

El fixture `pruebas-tanda-09-preserve-none-indirecta-matriz` compara llamadas directas e indirectas con Clang 23.1.0 en bibliotecas estáticas y DLL a O0–O3. Pasaron ocho comparaciones (`A:/temp/nsc-t09-preserve-all-escalares-retorno-1vh7ral8`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-all-escalares-retorno-precompilado-oag0ou3y`). En O0, tanto estática como DLL ejecutaron una primera compilación sin entrada y otra con `compilation-bundle-cache: hit`, verificando el ejecutable en ambos casos. La prueba negativa de convención incompatible permaneció aprobada (`A:/temp/nsc-t09-preserve-none-callback-negativo-0zdeu__0`). La compilación incremental pasó.

La evidencia corresponde a estas firmas y al destino Windows x64. No acredita todas las formas de callback, agregados, variádicas ni TLS aislado; T09-003 y la tanda 9 siguen abiertos.
