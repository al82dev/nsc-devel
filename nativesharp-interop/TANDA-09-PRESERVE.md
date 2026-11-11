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

La cobertura indirecta de firmas mixtas se detalla abajo. Otras firmas,
conversiones de lambdas y metodos, y los demas casos de T09-003 quedan
pendientes. Esta evidencia no cierra T09-003 ni la tanda 9.

## Firmas mixtas por puntero

La misma muestra escalar C directa se probo mediante punteros devueltos por C:
`preserve_most` recibe `double`, `int*`, `short`, booleano y retorna `double`;
`preserve_all` recibe entero de 64 bits, entero sin signo y `float`, y retorna
entero de 64 bits. Los ocho controles C pasaron
(A:/temp/nsc-t09-preserve-indirecta-mixta-nciy8qsa); NSC rechazaba la firma
(A:/temp/nsc-t09-preserve-indirecta-mixta-2ys76sra). Clang 23.1.0 emitio las
convenciones y `zeroext` del booleano en A:/temp/t09-preserve-ptr-mixed-oracle.ll.

Ahora pasan las ocho comparaciones C/NS, la inspeccion LLVM de las llamadas
indirectas y `zeroext`, y la cache
(A:/temp/nsc-t09-preserve-indirecta-mixta-1aygb8be), junto con 16 consumidores
precompilados con y sin encabezado
(A:/temp/nsc-t09-preserve-indirecta-mixta-precompilado-im7kps6e). La muestra
comprueba tambien la escritura por el puntero y valores de 64 bits. La muestra
indirecta anterior de enteros paso de nuevo
(A:/temp/nsc-t09-preserve-indirecta-ne8o69x3). Los demas tipos y firmas
indirectas permanecen pendientes; T09-003 y la tanda 9 siguen abiertos.

## Alias C++ preserve_most por puntero

Se contrasto una biblioteca C++ con un alias de puntero `preserve_most` en un
namespace, una funcion que devuelve ese puntero y la conversion de una funcion
compatible. La firma recibe `double`, `int*`, `short` y `bool`, y retorna
`double`. Clang genero los nombres decorados y la convencion observados en
A:/temp/t09-preserve-cpp-pointer-oracle.ll. El primer fixture uso por error el
nombre `get`, reservado por NativeSharp; se sustituyo por `fetchCallback` antes
de las pruebas definitivas. No fue necesario modificar el compilador.

Pasaron ocho comparaciones C++/NS con biblioteca estatica y DLL O0-O3,
comprobando ambos simbolos, la direccion de la funcion, la llamada indirecta,
`zeroext` y la escritura por el puntero
(A:/temp/nsc-t09-preserve-cpp-indirecta-609wsxuh). Pasaron 16 consumidores de
plantilla precompilada con y sin encabezado
(A:/temp/nsc-t09-preserve-cpp-indirecta-precompilado-cix1grik). En esta ruta
C++ la cache informa `disabled`; no se afirma una prueba de acierto de cache.
Otras firmas y convenciones siguen pendientes; T09-003 permanece abierto.

## Retornos void y puntero

Cuatro funciones C `preserve_most` y `preserve_all` cubren ambos retornos
`void` e `int*`, con parametros `int*` e `int`. Se prueban llamadas directas,
punteros devueltos por C y conversiones de funciones compatibles a puntero.
El control C paso estatica/DLL O0-O3
(A:/temp/nsc-t09-preserve-retornos-e9y0ynj3), pero NSC rechazaba el retorno
(A:/temp/nsc-t09-preserve-retornos-8nneafq7). El IR de Clang registra las
ocho llamadas y sus convenciones en A:/temp/t09-preserve-retornos-oracle.ll.

Ahora ambas rutas de NSC admiten esos retornos. Pasaron las ocho comparaciones
C/NS, comprobaciones del IR y cache
(A:/temp/nsc-t09-preserve-retornos-bceyiii6), y 16 consumidores precompilados
con y sin encabezado (A:/temp/nsc-t09-preserve-retornos-precompilado-4i25_03s).
Las pruebas comprueban escritura, direccion de las funciones y del puntero
retornado. Pasaron la regresion de firmas mixtas
(A:/temp/nsc-t09-preserve-indirecta-mixta-4f706a13) y los rechazos de
conversiones incompatibles (A:/temp/nsc-t09-preserve-indirecta-conversiones-ml6p0nyt).
Otros tipos de retorno y firmas permanecen pendientes; T09-003 sigue abierto.

## Retornos booleanos

Una muestra C recibe y retorna `_Bool` en llamadas directas e indirectas con
`preserve_most` y `preserve_all`; comprueba ambos valores y la direccion de las
funciones. El control C paso estatica/DLL O0-O3
(A:/temp/nsc-t09-preserve-bool-20rmex79), mientras NSC rechazaba el retorno
(A:/temp/nsc-t09-preserve-bool-1231ia6z). Clang aplica `zeroext` al argumento
y al retorno (A:/temp/t09-preserve-bool-oracle.ll).

Ahora pasan ocho comparaciones C/NS, verificaciones de ambas extensiones en
llamadas directas e indirectas y cache (A:/temp/nsc-t09-preserve-bool-ql7ejjy9),
y 16 consumidores precompilados con y sin encabezado
(A:/temp/nsc-t09-preserve-bool-precompilado-lxcop106). Pasaron las regresiones
de retornos `void` y `int*` (A:/temp/nsc-t09-preserve-retornos-k7m3lffp) y de
conversiones incompatibles (A:/temp/nsc-t09-preserve-indirecta-conversiones-bh0hv6l1).
Otros tipos de retorno permanecen pendientes; T09-003 y la tanda 9 siguen abiertos.

## Enteros estrechos

Se probaron parametros y retornos `signed char`, `unsigned char`, `short` y
`unsigned short` en llamadas C directas e indirectas con ambas convenciones.
Los controles C pasaron en A:/temp/nsc-t09-preserve-most-estrechos-40txmmba y
A:/temp/nsc-t09-preserve-all-estrechos-pdni4q3m; NSC rechazaba las firmas en
A:/temp/nsc-t09-preserve-most-estrechos-2yar3821 y
A:/temp/nsc-t09-preserve-all-estrechos-d91r4087. Clang emitio `i8` e `i16`
sin extensiones adicionales en A:/temp/t09-preserve-most-estrechos-oracle.ll
y A:/temp/t09-preserve-all-estrechos-oracle.ll.

Pasaron ocho comparaciones C/NS por convencion, con verificacion LLVM y cache
(A:/temp/nsc-t09-preserve-most-estrechos-djittsme y
A:/temp/nsc-t09-preserve-all-estrechos-_grp1d38), y 16 consumidores
precompilados por convencion con y sin encabezado
(A:/temp/nsc-t09-preserve-most-estrechos-precompilado-ib4ms_8r y
A:/temp/nsc-t09-preserve-all-estrechos-precompilado-pprvq_kq). Tambien paso la
regresion booleana (A:/temp/nsc-t09-preserve-bool-nfx__k8b). Otras firmas y
tipos quedan pendientes; T09-003 y la tanda 9 siguen abiertos.

## Retornos escalares adicionales

Se contrastaron firmas C con parametros y retornos `float`, `unsigned int` y
`unsigned long long`, ademas de una regresion `double`, en llamadas directas e
indirectas de ambas convenciones. Los controles C pasaron en
A:/temp/nsc-t09-preserve-most-escalares-retorno-ivfq0nge y
A:/temp/nsc-t09-preserve-all-escalares-retorno-5prl_r5k; NSC rechazaba las
firmas en A:/temp/nsc-t09-preserve-most-escalares-retorno-nqjm703c y
A:/temp/nsc-t09-preserve-all-escalares-retorno-1mg1hs17. Clang describio las
firmas en A:/temp/t09-preserve-most-scalar-oracle.ll y
A:/temp/t09-preserve-all-scalar-oracle.ll.

Pasaron ocho comparaciones C/NS por convencion, IR y cache
(A:/temp/nsc-t09-preserve-most-escalares-retorno-vfj2f021 y
A:/temp/nsc-t09-preserve-all-escalares-retorno-w_st5122), y 16 consumidores
precompilados por convencion con y sin encabezado
(A:/temp/nsc-t09-preserve-most-escalares-retorno-precompilado-3lth7mdv y
A:/temp/nsc-t09-preserve-all-escalares-retorno-precompilado-6ermeydz).
Las muestras incluyen enteros sin signo cercanos al limite de 64 bits.
Pasaron regresiones booleanas, estrechas y de retornos `void`/`int*`
(A:/temp/t09-preserve-scalar-reg-*.log). Otras firmas y tipos siguen pendientes;
T09-003 y la tanda 9 permanecen abiertos.

## Punteros a otros tipos

Una muestra C recibe `unsigned char*`, `short*` y `void*`, y retorna los dos
punteros tipados, mediante llamadas directas e indirectas `preserve_most` y
`preserve_all`. Comprueba identidad de punteros y funciones, y escritura en la
memoria apuntada. Los controles C pasaron
(A:/temp/nsc-t09-preserve-punteros-wokbnysd), pero NSC rechazaba esas firmas
(A:/temp/nsc-t09-preserve-punteros-584uvhqw); el IR de Clang esta en
A:/temp/t09-preserve-pointers-oracle.ll.

Los filtros de ambas rutas reconocen ahora punteros por su identidad, en vez de
limitarse a `int*`. Pasaron ocho comparaciones C/NS, IR y cache
(A:/temp/nsc-t09-preserve-punteros-wxo8_2qd) y 16 consumidores precompilados
con y sin encabezado (A:/temp/nsc-t09-preserve-punteros-precompilado-a45o5tdz).
Pasaron regresiones de `int*` y firmas mixtas
(A:/temp/nsc-t09-preserve-retornos-ar3k5utt y
A:/temp/nsc-t09-preserve-indirecta-mixta-twdz1c9l). No se acredita con estas
pruebas la interoperabilidad de todos los tipos apuntados; la seccion siguiente
detalla una muestra de record y puntero doble. T09-003 y la tanda 9 siguen abiertos.

## Punteros a record y punteros dobles

Una muestra C pasa y retorna un puntero a un record de dos enteros y un puntero
doble `int**`, mediante llamadas directas e indirectas en ambas convenciones.
Comprueba la direccion de los punteros y las funciones, y modifica el record y
el entero por sus respectivos punteros. No se requirieron cambios de produccion.

Pasaron los ocho controles C (A:/temp/nsc-t09-preserve-recordptr-8hgmlxwn),
ocho comparaciones C/NS en estatica y DLL O0-O3, inspeccion LLVM y cache
(A:/temp/nsc-t09-preserve-recordptr-ngpsialy), y 16 consumidores precompilados
con y sin encabezado (A:/temp/nsc-t09-preserve-recordptr-precompilado-fsd7t9lt).
Otras clases de record, punteros y firmas permanecen sin certificar; T09-003 y
la tanda 9 siguen abiertos.

## Records C por valor

Una muestra C pasa y retorna records de dos y seis enteros, en llamadas
directas e indirectas con `preserve_most` y `preserve_all`. Los ocho controles
C pasaron (A:/temp/nsc-t09-preserve-records-5263eb0v); NSC rechazaba la firma
(A:/temp/nsc-t09-preserve-records-xkq_mxkl). Clang representa el record de
8 bytes con `i64` y el de 24 bytes mediante copia y `sret`
(A:/temp/t09-preserve-records-oracle.ll).

NSC usa ahora el ABI Win64 existente para los records identificados por su
modelo en estas convenciones. Pasaron ocho comparaciones C/NS, IR con `i64`
y `sret`, conservacion del valor original y cache
(A:/temp/nsc-t09-preserve-records-nhd5a02i), y 16 consumidores precompilados
con y sin encabezado (A:/temp/nsc-t09-preserve-records-precompilado-koagjzhv).
Pasaron regresiones de punteros a record, booleanos y agregados C previos
(A:/temp/t09-preserve-records-reg-punteros.log,
A:/temp/t09-preserve-records-reg-bool.log y
A:/temp/t09-preserve-records-reg-anteriores.log). La primera revision del
verificador de `sret` fallo por un patron incompleto para el nombre de funcion;
se corrigio el verificador y paso la matriz completa. Los layouts y firmas no
probados siguen pendientes; T09-003 y la tanda 9 permanecen abiertos.

### Records de 3 y 4 bytes

Se contrastaron tambien un record C de 4 bytes y otro de 3 bytes, pasados y
retornados por valor con ambas convenciones. Pasaron los ocho controles C
(A:/temp/nsc-t09-preserve-records34-vs8t0f0r), ocho comparaciones C/NS en
estatica y DLL O0-O3, IR con `i32` y `sret` de 3 bytes, preservacion del
original y cache (A:/temp/nsc-t09-preserve-records34-e11wvs77), y 16
consumidores precompilados con y sin encabezado
(A:/temp/nsc-t09-preserve-records34-precompilado-pigtd7cd). No hizo falta
modificar produccion. Estos controles no certifican todos los layouts.
