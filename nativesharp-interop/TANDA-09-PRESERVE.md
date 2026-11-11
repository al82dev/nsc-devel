# T09-003: preserve_most y preserve_all en Windows x64

Clang 23.1.0 emite `preserve_mostcc` y `preserve_allcc` para las muestras
escalares C (`A:/temp/t09-convention-oracle.ll`). Los controles C pasaron
estatica/DLL O0-O3 (`A:/temp/nsc-t09-preserve-z9gg0mbb`) y NSC rechazaba
`preserve_most` (`A:/temp/nsc-t09-preserve-k5491bin`). Ahora la convencion
se conserva en HIR, MIR, cache, declaracion y llamada LLVM para las firmas
de `int` comprobadas. Pasaron ocho comparaciones C/NS, simbolo, IR y cache
(`A:/temp/nsc-t09-preserve-7q65uuru`), y 16 consumidores precompilados con
y sin encabezado (`A:/temp/nsc-t09-preserve-precompilado-gbco_aal`).

Una conversion a puntero `cdecl` se aceptaba indebidamente
(`A:/temp/nsc-t09-preserve-conversiones-nc66dqyf`). Las conversiones desde
ambas convenciones se rechazan hasta implementar su identidad de puntero
(`A:/temp/nsc-t09-preserve-conversiones-z_lhphyc`); tambien pasaron los
controles de conversion `regcall` (`A:/temp/nsc-t09-regcall-conversiones-x0mn_d5l`).

La muestra C++ `preserve_most` en namespace paso ocho comparaciones C++/NS
(`A:/temp/nsc-t09-preserve-cpp-25xgxuvp`) y 16 consumidores precompilados
(`A:/temp/nsc-t09-preserve-cpp-precompilado-6k4n7sx7`). Clang 23.1.0 rechazo
decorar el nombre C++ de la muestra `preserve_all` en este destino; el control
reproducible conserva el diagnostico
(`A:/temp/nsc-t09-preserve-cpp-limite-a_scrf3j`). No se modifico Clang ni se
afirma soporte C++ para ese caso.

Pasaron regresiones `cdecl` (`A:/temp/nsc-t09-llamadas-a_io0j3n`), `regcall`
precompilado (`A:/temp/nsc-t09-regcall-estrechos-precompilado-8ere5j_v`),
cuatro sondas (`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). Otras firmas y llamadas por puntero
permanecen pendientes; T09-003 y la tanda 9 siguen abiertos.

## Firmas escalares mixtas

Clang clasifico las muestras C con `double`, `float`, enteros, booleano y
`int*` en A:/temp/t09-preserve-scalars-oracle.ll. El control C paso
estatica/DLL O0-O3 (A:/temp/nsc-t09-preserve-escalares-1hy5zckw), mientras
NSC rechazo la firma mixta (A:/temp/nsc-t09-preserve-escalares-_zhs211q).
Tras ampliar el filtro de firmas, pasaron ocho comparaciones C/NS, la
verificacion LLVM de ambas convenciones y `zeroext`, y cache
(A:/temp/nsc-t09-preserve-escalares-m1udhb7m), junto con 16 consumidores
precompilados (A:/temp/nsc-t09-preserve-escalares-precompilado-dw1pq0t7).
La muestra comprueba la escritura mediante el puntero y valores de 64 bits.

La firma mixta C++ `preserve_most` paso ocho comparaciones C++/NS y la
comprobacion del atributo booleano
(A:/temp/nsc-t09-preserve-cpp-escalares-rjlaj17w), y 16 consumidores
precompilados (A:/temp/nsc-t09-preserve-cpp-escalares-precompilado-q5_hsm1e).
Pasaron las firmas anteriores (A:/temp/nsc-t09-preserve-n8lkjlab), rechazo
de conversiones incompatibles (A:/temp/nsc-t09-preserve-conversiones-5wo2k9_s),
cuatro sondas (A:/temp/t09-test-varptr.log) y transporte/ABI/identidad
(A:/temp/t09-transport-directed/). Otras firmas y punteros a funcion siguen
pendientes; T09-003 permanece abierto.
