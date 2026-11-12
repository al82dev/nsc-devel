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

### Dependencia: record C++ trivial por valor

En el diagnostico inicial, una muestra C++ con `T09Cpp::Small`, un alias de puntero y una funcion
`preserve_most` recibe y devuelve el record de dos enteros por valor. Clang
23.1.0 genero el simbolo y ABI en A:/temp/t09-preserve-cpp-record-oracle.ll;
pasaron ocho controles C++ en estatica/DLL O0-O3
(A:/temp/nsc-t09-preserve-cpp-record-xj4ocsak). NSC no completo la importacion
de las funciones y rechazo copiar el argumento al puntero de funcion
(A:/temp/nsc-t09-preserve-cpp-record-0wqp3d4a); el consumidor precompilado
tampoco paso (A:/temp/nsc-t09-preserve-cpp-record-precompilado-zldwpu8v).

En ese momento la proyeccion de funciones en `src/Interop/Cpp/CppSemanticFunction.cpp`
restringia los records por valor a origen C, y la representacion y vida de
objetos C++ corresponden a T11-002, T11-008 y tareas relacionadas. Esta es una
dependencia no lineal: hay que resolver esa representacion y validar la muestra
antes de acreditar este caso. Los scripts en
`pruebas-tanda-09-preserve-cpp-record` conservan el caso reproducible; las
pruebas NS y precompilada estaban pendientes. Los resultados posteriores estan
en las secciones siguientes. La
funcionalidad de records C por valor no demuestra el caso C++.

Como primer paso de esa dependencia, la observacion de records C++ completos
conserva ahora desde Clang si son trivialmente copiables, tienen layout
estandar, constructor predeterminado trivial y destructor trivial. Esos datos
entonces no se aplicaban a la importacion por valor y no se serializan en NSMeta; los
records incompletos y los records C no reportan esta observacion. La prueba
independiente cubre un tipo trivial, constructor/copia/destructor no triviales,
layout no estandar, declaracion incompleta, snapshot tras cierre y record C
(A:/temp/t09-cpp-record-traits-test.log). Tambien comprueba un tipo con copia
eliminada que Clang considera trivialmente copiable: ese rasgo por si solo no
demuestra que la copia sea accesible o valida.

Se recompilaron las sondas afectadas; pasaron las cuatro sondas y los controles
de transporte/ABI/identidad, junto con 16 consumidores precompilados de records
C y 16 de funciones C++ escalares (A:/temp/t09-cpp-record-traits-reg-*.log).
En esa etapa la muestra de record C++ por valor seguia pendiente: faltaban reglas de
seleccion y accesibilidad de operaciones, proyeccion de la firma, transporte
del contrato vigente y ejecucion comprobada. No se declara compatibilidad por
la observacion de estos rasgos.

La sesion Clang expone ahora una consulta viva de seleccion de constructor de
copia para argumentos const y no const, y del destructor. Devuelve seleccion,
ausencia/eliminacion o ambiguedad, junto con el acceso declarado y trivialidad
cuando Clang identifica un miembro. El acceso declarado no equivale por si
solo a una decision de accesibilidad desde un contexto C++ concreto.

La prueba dirigida cubre copias implicitas publicas, copia eliminada, copia
privada, sobrecargas const/no const, sobrecarga ambigua, destructor privado y
eliminado, nombre ausente, record incompleto, lenguaje C y sesion cerrada
(A:/temp/t09-cpp-record-operations-test.log). Clang confirmo el caso ambiguo
(A:/temp/t09-copy-ambiguous.log). Pasaron cuatro sondas y controles de
transporte/ABI/identidad, y 16 consumidores precompilados C y 16 C++
(A:/temp/t09-cpp-record-operations-reg-*.log). Esta consulta todavia no alimenta
la proyeccion de funciones ni NSMeta en esa etapa; el resultado posterior se
documenta mas abajo.

El modelo de tipos seleccionados de una compilacion conserva ahora estas
observaciones para records C++ con una sola definicion y layout disponible.
La consulta se realiza solo para tipos seleccionados y vincula el resultado al
proyecto, sesion, triple e identidad USR observados; no se incorpora ante
ausencia o ambiguedad. Los campos son observaciones vivas: no conceden permiso
de copia ni de llamada y no se serializan en NSMeta.

La prueba nueva verifica el transporte con una sesion real, triple, indice e
identidad incorrectos, proyecto ambiguo, record incompleto y sesion cerrada.
Tambien compila la seleccion de un proyecto real y comprueba que un record
normal y otro con copia eliminada conservan resultados distintos en el modelo
(A:/temp/t09-cpp-record-observation-final.log). Pasaron cuatro sondas,
transporte/ABI/identidad y 16 consumidores precompilados C y 16 C++
(A:/temp/t09-cpp-record-model-reg-*.log). Entonces faltaba aplicar reglas de uso y ABI,
proyectar las firmas, transportar el contrato vigente y ejecutar la muestra C++
por valor; los avances posteriores no cierran T09-003.

### Llamada directa con record C++ trivial

La muestra `T09Cpp::Small` de dos enteros se pasa y retorna por valor desde
NativeSharp mediante una funcion libre `preserve_most`. El control C++ paso
(A:/temp/nsc-t09-preserve-cpp-record-directo-kptoyziz); antes de la proyeccion
NSC no encontraba la funcion
(A:/temp/nsc-t09-preserve-cpp-record-directo-cju219ww). En la implementacion se
reutiliza el ABI Win64 para un record observado por Clang como POD y de layout
estandar, con constructor predeterminado, copias const/no const, asignaciones
const/no const y destructor publicos, triviales y no eliminados. Se excluyen
miembros o bases volatiles o no literales. La identidad, dimensiones, proyecto
y destino se contrastan antes de permitir la firma; no se aplica la regla si
falta alguna observacion.

Pasaron ocho comparaciones C++/NS en estatica y DLL O0-O3, con el simbolo y
convencion de Clang, el resultado y el original comprobados y sin llamar al
destructor trivial (A:/temp/nsc-t09-preserve-cpp-record-directo-920_y2m2).
Pasaron diez controles de rechazo de operaciones eliminadas, privadas o no
triviales y miembros volatiles (A:/temp/nsc-t09-cpp-record-rechazos-1lic8dr3).
La seleccion y rasgos tienen tres pruebas dirigidas
(A:/temp/t09-cpp-record-pod-tests-final.log). Una regresion de constructor y
destructor C++ no triviales comparo C++/NS en O0 y O2
(A:/temp/nsc-t09-cpp-nontrivial-reg-vjaou713). Pasaron cuatro sondas,
transporte/ABI/identidad y las regresiones de records C y plantillas C++
escalares (A:/temp/t09-cpp-pod-final-*.log).

La prueba precompilada aun falla: el productor informa que el origen del record
C++ no esta admitido en el contrato de metadatos vigente
(A:/temp/nsc-t09-preserve-cpp-record-directo-precompilado-ykd4ztzr). Otros tipos
C++ no triviales no se acreditan; T09-003 sigue abierto.

### Puntero a funcion C++ con record trivial

La muestra con `T09Cpp::Small` por valor admite ahora un puntero
`preserve_most` devuelto por C++, su invocacion y la conversion de una funcion
compatible al mismo tipo. El modelo aplica a los records C++ la regla de
identidad, layout y operaciones triviales de la llamada directa; la regla para
records C conserva sus condiciones anteriores. El soporte se limita a la
forma y convencion comprobadas, no a todos los punteros o records C++.

Pasaron ocho comparaciones con el programa C++ en estatica y DLL O0-O3; se
comprobaron el resultado, la copia, la identidad del puntero y la convencion
indirecta en IR (A:/temp/nsc-t09-preserve-cpp-record-j3lpu_gd). Diez muestras
con operaciones privadas, eliminadas o no triviales y miembros volatiles
producen el diagnostico esperado de copia indirecta
(A:/temp/nsc-t09-cpp-record-ptr-rechazos-47g76nxa). Estos rechazos comprueban
el uso integrado, no de manera aislada cada paso de proyeccion.

La compilacion incremental y cuatro sondas pasaron, asi como los controles de
transporte, ABI e identidad y el rechazo de conversiones incompatibles de
punteros escalares C (A:/temp/t09-cpp-ptr-final-*.log). Pasaron ademas ocho
comparaciones de records C, ocho de llamada C++ directa y 16 consumidores
precompilados de punteros C++ escalares (A:/temp/t09-cpp-record-ptr-*.log).
La prueba precompilada con este record C++ sigue fallando en el productor por
el origen del record en metadatos
(A:/temp/nsc-t09-preserve-cpp-record-precompilado-h3_su9mc). Esa capacidad y
T09-003 permanecen pendientes.
