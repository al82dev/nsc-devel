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
ambas convenciones se rechazaban mientras faltaba su identidad de puntero
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
(`A:/temp/t09-transport-directed/`). La cobertura posterior de punteros se
detalla abajo; T09-003 y la tanda 9 siguen abiertos.

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
(A:/temp/t09-transport-directed/). Otras firmas siguen pendientes;
la cobertura de punteros se detalla abajo. T09-003 permanece abierto.

## Punteros preserve devueltos por C

Los controles C de punteros con tres parametros y retorno `int` pasaron en
estatica/DLL O0-O3 (A:/temp/nsc-t09-preserve-indirecta-j1bm45nh), mientras
NSC rechazaba el tipo importado (A:/temp/nsc-t09-preserve-indirecta-blvrpy2i).
Ahora las identidades de funcion conservan ambas convenciones, incluyendo
sus formas explicitas `fn* preserve_most` y `fn* preserve_all`. La llamada
indirecta emite la convencion LLVM correspondiente. Las conversiones desde
funciones importadas solo se admiten cuando coincide la convencion.

Pasaron ocho comparaciones C/NS, verificaciones del IR y cache en estatica y
DLL (A:/temp/nsc-t09-preserve-indirecta-08_85ltk), y 16 consumidores de una
plantilla precompilada con y sin encabezado
(A:/temp/nsc-t09-preserve-indirecta-precompilado-stuf8_fe). Se rechazaron las
conversiones entre ambas convenciones y con `cdecl`, en los dos sentidos
(A:/temp/nsc-t09-preserve-indirecta-conversiones-vxijs7r2); tambien paso la
prueba anterior (A:/temp/nsc-t09-preserve-conversiones-_eo23f71).
Pasaron conversiones y precompilacion `regcall`, cuatro sondas, transporte,
ABI, identidad y metadatos (registros A:/temp/t09-preserve-ptr-*.log).

La cobertura indirecta se limita a las firmas `int` comprobadas. Otras
firmas, conversiones de lambdas y metodos, y los demas casos de T09-003
quedan pendientes. Esta evidencia no cierra T09-003 ni la tanda 9.
