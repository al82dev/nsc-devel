# T09-003: regcall escalar directo en Windows x64

Clang oficial 23.1.0 emite `x86_regcallcc` y el simbolo
`__regcall3__t09_regcall` para el control de tres argumentos `int` y retorno
`int` (`A:/temp/t09-convention-oracle.ll`). El control C independiente paso
estatica y DLL en O0-O3 (`A:/temp/nsc-t09-regcall-y71k72p2`), mientras NSC
rechazaba la convencion (`A:/temp/nsc-t09-regcall-wixfx9br`).

La sesion obtiene el simbolo de CodeGen de Clang; HIR y MIR transportan la
convencion e incluyen el dato en identidades y serializacion de cache. LLVM
aplica `x86_regcallcc` a declaracion y llamada. El control comprueba simbolo
en el objeto, llamada LLVM, ejecucion con tres valores diferentes y cache
miss/hit en estatica y DLL. Pasaron las ocho comparaciones C/NativeSharp O0-O3
(`A:/temp/nsc-t09-regcall-pvsd9fxk`) y 16 consumidores de plantilla con y
sin encabezado (`A:/temp/nsc-t09-regcall-precompilado-ndl5ojoc`).

Una firma con argumento `double` se rechaza
(`A:/temp/nsc-t09-regcall-negativo-9ykay9qz`); no se acreditan otros tipos,
records, punteros a funcion, DLL con aislamiento TLS ni otras convenciones.
Pasaron regresiones cdecl (`A:/temp/nsc-t09-llamadas-67a6q_9a`), vectorcall
(`A:/temp/nsc-t09-vectorcall-k8y58pzj`), SysV precompilado
(`A:/temp/nsc-t09-sysv-indirecta-opcional-flotante-pre-5_ukljdt`), cuatro
sondas (`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). T09-003 y la tanda permanecen abiertos.

## Enteros de distintas anchuras

Una muestra con `unsigned int`, `long long` y `unsigned long long` de
parametros y retorno `unsigned long long` conserva valores negativos y el bit
alto. El control C paso estatica/DLL O0-O3
(`A:/temp/nsc-t09-regcall-enteros-24onjjoy`) y NSC rechazo inicialmente la
firma (`A:/temp/nsc-t09-regcall-enteros-4u22keva`). Tras ampliar el filtro a
enteros de 32 y 64 bits con y sin signo, pasaron ocho comparaciones C/NS,
IR, simbolo y cache (`A:/temp/nsc-t09-regcall-enteros-zwvfgqyn`) y 16
consumidores precompilados con y sin encabezado
(`A:/temp/nsc-t09-regcall-enteros-precompilado-nr9qolj4`). Pasaron la muestra
anterior (`A:/temp/nsc-t09-regcall-bwl9lztu`), rechazo de `double`
(`A:/temp/nsc-t09-regcall-negativo-eqeddmfj`), cuatro sondas
(`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). La muestra no acredita todas las firmas;
T09-003 permanece abierto.

## Retornos void y puntero en regcall

Los controles C pasaron y NSC rechazo inicialmente el retorno `int*` en
llamadas directas (`A:/temp/nsc-t09-regcall-void-puntero-amhtiwcj`,
`A:/temp/nsc-t09-regcall-void-puntero-pfzplqzs`). Tras admitir `int*` y
`void`, pasaron ocho comparaciones C/NS, IR y cache
(`A:/temp/nsc-t09-regcall-void-puntero-61m886zq`) y 16 consumidores
precompilados (`A:/temp/nsc-t09-regcall-void-puntero-precompilado-elf3h373`).

Los controles equivalentes por puntero pasaron en C y fueron rechazados por
NSC antes de la ampliacion (`A:/temp/nsc-t09-regcall-indirecta-vp-yqid7th5`,
`A:/temp/nsc-t09-regcall-indirecta-vp-8xey612k`). Pasaron despues ocho
comparaciones, IR y cache (`A:/temp/nsc-t09-regcall-indirecta-vp-dtscx3ha`)
y 16 consumidores precompilados
(`A:/temp/nsc-t09-regcall-indirecta-vp-precompilado-5iulw2q5`). Las muestras
comprueban identidad del puntero y efectos sobre el dato. Pasaron rechazo de
conversiones y firma `double` no admitida
(`A:/temp/nsc-t09-regcall-conversiones-egrolnqk`), cuatro sondas
(`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). Otras firmas siguen pendientes; T09-003
permanece abierto.

## Enteros estrechos en regcall

Clang emite `i8` e `i16` sin atributos de extension para las muestras
(`A:/temp/t09-regcall-narrow-oracle.ll`). El control C paso O0-O3 en estatica
y DLL (`A:/temp/nsc-t09-regcall-estrechos-649xkt5j`), pero NSC rechazaba la
firma (`A:/temp/nsc-t09-regcall-estrechos-g8hilae1`). Tras admitir `sbyte`,
`byte`, `short` y `ushort`, pasaron ocho comparaciones C/NS con llamadas
directas y por puntero, IR y cache
(`A:/temp/nsc-t09-regcall-estrechos-zu6mcinp`) y 16 consumidores
precompilados (`A:/temp/nsc-t09-regcall-estrechos-precompilado-_nnt1jnq`).
Las muestras comprueban valores negativos y valores altos sin signo.

Pasaron el rechazo de `double` directo e indirecto y conversiones incompatibles
(`A:/temp/nsc-t09-regcall-negativo-atzr6uv4`,
`A:/temp/nsc-t09-regcall-conversiones-7t268yfq`), la regresion precompilada
de retornos `void` e `int*`
(`A:/temp/nsc-t09-regcall-indirecta-vp-precompilado-vpj1fm9_`), cuatro
sondas (`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). Las muestras no acreditan todas las
firmas; T09-003 permanece abierto.

## Booleanos en regcall

Clang emite retorno y parametro `zeroext i1` para la muestra C
(`A:/temp/t09-regcall-bool-oracle.ll`). El control C paso estatica/DLL O0-O3
(`A:/temp/nsc-t09-regcall-bool-mo0_8jfu`) y NSC rechazaba la firma
(`A:/temp/nsc-t09-regcall-bool-sf5c4q5d`). Tras admitir `bool`, pasaron ocho
comparaciones C/NS directas e indirectas, atributos LLVM y cache
(`A:/temp/nsc-t09-regcall-bool-49mk19wx`) y 16 consumidores precompilados
(`A:/temp/nsc-t09-regcall-bool-precompilado-4pjozy4x`).

Clang usa los mismos atributos para la funcion C++ en namespace
(`A:/temp/t09-regcall-cpp-bool-oracle.ll`). Pasaron ocho comparaciones C++/NS
y atributos de retorno y parametro
(`A:/temp/nsc-t09-regcall-cpp-bool-t5ygpqqm`) y 16 consumidores precompilados
(`A:/temp/nsc-t09-regcall-cpp-bool-precompilado-mv1zh0m4`). Pasaron el rechazo
de firmas `double` y conversiones incompatibles
(`A:/temp/nsc-t09-regcall-negativo-sund5oti`,
`A:/temp/nsc-t09-regcall-conversiones-5dolgxv6`), la regresion precompilada
de enteros estrechos
(`A:/temp/nsc-t09-regcall-estrechos-precompilado-zfzwi8eh`), cuatro sondas
(`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). La muestra C++ indirecta y otras firmas
siguen pendientes; T09-003 permanece abierto.

## Funcion libre C++ en namespace

Clang emitio el simbolo `?calculate@T09Cpp@@YwHHHH@Z` y `x86_regcallcc`
(`A:/temp/t09-regcall-cpp-oracle.ll`). El control C++ paso estatica/DLL O0-O3
(`A:/temp/nsc-t09-regcall-cpp-xy8jklqf`); NativeSharp tambien ejecuto las
ocho variantes y conservo simbolo y convencion en LLVM
(`A:/temp/nsc-t09-regcall-cpp-42868933`). La cache de paquete de compilacion
se informa desactivada en esta muestra C++; no se acredita hit de esa cache.

La plantilla precompilada fallaba al resolver `T09Cpp`, incluso cuando el
consumidor incluye el encabezado
(`A:/temp/nsc-t09-regcall-cpp-precompilado-805e785k`). Un control equivalente
con `cdecl` falla de la misma forma
(`A:/temp/nsc-t09-regcall-cpp-precompilado-y0wame_8`), por lo que no hay
evidencia de que el defecto fuera exclusivo de `regcall`. El catalogo vigente
ahora captura el enlace C++ y la identidad de la funcion seleccionada, y la
demanda calificada puede localizar el namespace del productor antes de
restaurar el simbolo. Pasaron los 16 consumidores de la prueba original con
`regcall` (`A:/temp/nsc-t09-regcall-cpp-precompilado-j6gxcq9k`) y los 16 del
control `cdecl` (`A:/temp/nsc-t09-cpp-cdecl-precompilado-ik9rmc6i`), con y
sin encabezado, en estatica/DLL O0-O3. Pasaron tambien los controles C
precompilados (`A:/temp/nsc-t09-precompilado-yc3jre_f` y
`A:/temp/nsc-t09-regcall-precompilado-_knqkzxp`), las cuatro sondas de ABI
(`A:/temp/t09-test-varptr.log`) y las pruebas de transporte, ABI e identidad
(`A:/temp/t09-transport-directed/`), incluida la comprobacion nueva de enlace
y nombre C++ en el codec. Pasaron ademas las pruebas de metadatos y cierre de
dependencias seleccionadas (`A:/temp/t09-cpp-meta-check.log`). El control adicional con llamada propia
del consumidor y encabezado paso; sin encabezado no expone automaticamente
la dependencia privada de la plantilla
(`A:/temp/nsc-t09-regcall-cpp-precompilado-lwtb9ay5`). No se acreditan todos
los tipos ni todas las sobrecargas C++; T09-003 sigue abierto.

## Sobrecargas C++ en plantilla precompilada

Dos funciones `T09Cpp::calculate`, que difieren en el primer parametro `int`
o `long long`, tienen simbolos distintos segun Clang
(`A:/temp/t09-overload-oracle.ll`). El control C++ y la plantilla NativeSharp
comprueban resultados distintos para ambas sobrecargas. Pasaron las ocho
variantes de control y 16 consumidores precompilados, con y sin encabezado,
en estatica y DLL O0-O3
(`A:/temp/nsc-t09-cpp-sobrecargas-precompilado-73lootny`). No se cambio
produccion; otras formas de sobrecarga, tipos y convenciones siguen pendientes.
T09-003 permanece abierto.

## Float y puntero en regcall

El control C paso estatica/DLL O0-O3
(`A:/temp/nsc-t09-regcall-float-puntero-0qpooymq`) y NSC rechazaba la firma
con dos `float`, un `int*` y retorno `float`
(`A:/temp/nsc-t09-regcall-float-puntero-un0ee76o`). Tras ampliar el filtro
HIR, pasaron las ocho comparaciones C/NS, IR, simbolo y cache
(`A:/temp/nsc-t09-regcall-float-puntero-64_wfvfn`) y 16 consumidores
precompilados con y sin encabezado
(`A:/temp/nsc-t09-regcall-float-puntero-precompilado-b56jn89c`). La prueba
comprueba los argumentos, retorno y escritura mediante el puntero.

La misma firma C++ en namespace usa el simbolo observado por Clang
(`A:/temp/t09-regcall-cpp-fp-oracle.ll`) y paso ocho comparaciones C++/NS
(`A:/temp/nsc-t09-regcall-cpp-float-puntero-f3w4o9fg`) y 16 consumidores
precompilados (`A:/temp/nsc-t09-regcall-cpp-float-puntero-precompilado-ji6qhdla`).
Los primeros controles C++ fallaron por enlace de `_fltused`; se corrigio el
enlace C del simbolo en el fixture y se repitieron
(`A:/temp/nsc-t09-regcall-cpp-float-puntero-yll2oklv`,
`A:/temp/nsc-t09-regcall-cpp-float-puntero-precompilado-_a8lpqb7`).
Pasaron regresiones de enteros (`A:/temp/nsc-t09-regcall-enteros-kskuz4s0`),
rechazo de `double` (`A:/temp/nsc-t09-regcall-negativo-ph6jkfwk`), cuatro
sondas (`A:/temp/t09-test-varptr.log`) y transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`). Otras firmas y punteros siguen pendientes;
T09-003 permanece abierto.

## Puntero regcall devuelto por C

El control C paso estatica/DLL O0-O3
(`A:/temp/nsc-t09-regcall-indirecta-5sxm0ld1`), pero NSC rechazaba la
convencion del typedef (`A:/temp/nsc-t09-regcall-indirecta-t2aez33f`). La
identidad estructural y la escritura `fn* regcall` conservan la convencion;
LLVM la aplica a la llamada indirecta. Pasaron ocho comparaciones C/NS,
igualdad y llamada por punteros, IR y cache
(`A:/temp/nsc-t09-regcall-indirecta-3774fxgw`), asi como 16 consumidores
precompilados con y sin encabezado
(`A:/temp/nsc-t09-regcall-indirecta-precompilado-1o9d0nwq`). Se rechazaron
conversiones incompatibles con `cdecl` en ambas direcciones y la firma
indirecta `double` aun no implementada
(`A:/temp/nsc-t09-regcall-conversiones-lktx86es`).

La escritura explicita dentro de una plantilla descubrio una dependencia
generica falsa (`A:/temp/nsc-t09-regcall-indirecta-precompilado-0hzrnpxc`).
El colector recorre ahora parametros, resultado y error de tipos funcion sin
tratar el tipo estructural como plantilla nominal. Una prueba con tipo
generico no publico en parametro y retorno comprobo su inclusion en el
catalogo y ejecuto 16 consumidores
(`A:/temp/nsc-t09-funcion-cierre-generico-ljqr50g2`). Pasaron regresiones de
regcall directo (`A:/temp/nsc-t09-regcall-enteros-bo1diq3p`), sobrecargas C++
precompiladas (`A:/temp/nsc-t09-cpp-sobrecargas-precompilado-o_az4yhi`),
cuatro sondas (`A:/temp/t09-test-varptr.log`), transporte/ABI/identidad
(`A:/temp/t09-transport-directed/`) y metadatos
(`A:/temp/t09-cpp-meta-check.log`). Otras firmas indirectas quedan pendientes;
T09-003 permanece abierto.
