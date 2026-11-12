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

En la primera etapa una firma con argumento `double` se rechazaba
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
(`A:/temp/t09-transport-directed/`). Al momento de esta prueba, la muestra C++
indirecta seguia pendiente; el siguiente apartado documenta su validacion.

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

## Alias C++ de puntero regcall precompilado

La funcion C++ en namespace que devuelve un puntero booleano `regcall` paso
ocho comparaciones C++/NS y atributos LLVM
(`A:/temp/nsc-t09-regcall-cpp-bool-indirecta-6ig_dm5x`), pero la plantilla
rechazaba el alias (`A:/temp/nsc-t09-regcall-cpp-bool-indirecta-precompilado-sk7qoixg`).
El catalogo vigente captura ahora alias C++ de punteros a funcion; pasaron 16
consumidores con y sin encabezado
(`A:/temp/nsc-t09-regcall-cpp-bool-indirecta-precompilado-sjaps69z`).

Con el alias dentro del namespace, la llamada directa pasaba
(`A:/temp/nsc-t09-regcall-cpp-bool-indirecta-9ktdsyvy`), pero la superficie
de plantilla generada no reconocia el nombre calificado
(`A:/temp/nsc-t09-regcall-cpp-bool-indirecta-precompilado-0cle3e8q`). Al
incluir el nombre calificado como pista del parser, pasaron ocho comparaciones
directas (`A:/temp/nsc-t09-regcall-cpp-alias-calificado-omg_wyp7`) y 16
consumidores precompilados
(`A:/temp/nsc-t09-regcall-cpp-alias-calificado-precompilado-py1bis46`).
Pasaron regresiones del alias C
(`A:/temp/nsc-t09-regcall-bool-precompilado-2ofdr99t`), sobrecargas C++
(`A:/temp/nsc-t09-cpp-sobrecargas-precompilado-n9a9thxr`), transporte,
ABI e identidad (`A:/temp/t09-transport-directed/`) y metadatos
(`A:/temp/t09-cpp-meta-check.log`). Otros alias y firmas C++ siguen pendientes;
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

## Punteros a otros enteros en regcall

El control C con un argumento y retorno `short*` y otra funcion con retorno
`void` paso en estatica y DLL O0-O3
(A:/temp/nsc-t09-regcall-short-ptr-w5tyvw5i). Clang usa `x86_regcallcc` con
punteros en A:/temp/t09-regcall-short-pointer-oracle.ll; NSC rechazaba la
firma por restringirla a `int*`
(A:/temp/nsc-t09-regcall-short-ptr-ah5zg4z4).

La validacion de llamadas directas e indirectas admite ahora punteros que NSC
ya puede representar, sin habilitar escalares adicionales. Pasaron ocho
comparaciones C/NativeSharp para `short*`, con llamada directa y por puntero,
convencion y simbolo en IR, identidad y escritura
(A:/temp/nsc-t09-regcall-short-ptr-6e89t65y); pasaron 16 consumidores
precompilados con y sin encabezado
(A:/temp/nsc-t09-regcall-short-ptr-precompilado-ran1ep9y).

Las variantes `signed char*` y `unsigned char*` pasaron las mismas ocho
comparaciones y 16 consumidores precompilados por variante
(A:/temp/nsc-t09-regcall-byte-ptr-cy005l62). Se conserva y paso la prueba
existente que rechaza el `double` por valor no implementado
(A:/temp/nsc-t09-regcall-negativo-jxreeb45), y pasaron 16 consumidores de la
variante anterior `int*`
(A:/temp/nsc-t09-regcall-indirecta-vp-precompilado-mn91xvu2). Estas muestras
no certifican todas las formas de puntero ni otros tipos o destinos. T09-003
permanece abierto. Pruebas en
[pruebas-tanda-09-regcall-short-puntero](pruebas-tanda-09-regcall-short-puntero/).

## Punteros a record y punteros dobles en regcall

Una muestra C usa punteros a un record de dos enteros e `int**` como
argumentos y retornos, junto con funciones de retorno `void`. Las llamadas
directas y por puntero verifican convencion y simbolo, identidad y escritura
sobre el record y el dato apuntado. Pasaron ocho comparaciones C/NativeSharp
en estatica y DLL O0-O3 (A:/temp/nsc-t09-regcall-recordptr-akdiakwu), y 16
consumidores precompilados con y sin encabezado
(A:/temp/nsc-t09-regcall-recordptr-precompilado-2y7i43fp).

No se requirio cambiar produccion para estas muestras. No se acreditan todas
las formas de puntero ni se alteran los limites D23/D26; T09-003 permanece
abierto. Pruebas en
[pruebas-tanda-09-regcall-recordptr](pruebas-tanda-09-regcall-recordptr/).

## Planes de Clang para records por valor

Clang 23.1.0 clasifica un record de dos enteros como `i64` y otro de seis
enteros mediante retorno `sret` y argumento indirecto. Un record de dos
`float`, sin embargo, usa dos argumentos flotantes y un retorno estructurado
(A:/temp/t09-regcall-record-oracle.ll). El tamano del record por si solo no
autoriza usar el ABI ordinario Win64 en `regcall`.

La sesion puede consultar ahora el plan de Clang de un alias de funcion
`regcall`, y el inventario conserva el plan de funciones C directas con
record por valor. La prueba nueva contrasta ambas consultas para records de
ocho y 24 bytes; el plan actual no representa el record de dos `float` y no
se habilita por ello (A:/temp/t09-regcall-plan-final-plan.log). La comparacion
de consultas no es una prueba de ejecucion ni autoriza por si sola llamadas.

El control C de la muestra de records por valor paso en estatica y DLL O0-O3
(A:/temp/nsc-t09-regcall-records-1nk18nq5), pero en esa etapa NSC la rechazaba
(A:/temp/nsc-t09-regcall-records-jpdvbp6d). En esa etapa faltaba reconciliar y transportar
el plan hasta la emision directa e indirecta y la precompilacion; la prueba
reproducible esta en
[pruebas-tanda-09-regcall-records](pruebas-tanda-09-regcall-records/).
Pasaron cuatro sondas de ABI/punteros, transporte/ABI/identidad, 16
consumidores precompilados de punteros regcall y la prueba negativa de
`double` por valor (A:/temp/t09-regcall-plan-final-*.log). T09-003 sigue
abierto.

La reconciliacion de planes de puntero admite tambien `regcall`: comprueba la
convencion y el simbolo sintetico que Clang genera, la firma, identidad y
origen de los records. La prueba contrasta los records de enteros de ocho y
24 bytes con la funcion directa y rechaza nombre, tamano y procedencia
incompatibles (A:/temp/t09-regcall-plan-test.log). Pasaron las cuatro sondas
de ABI/punteros y controles de transporte, ABI e identidad
(A:/temp/t09-regcall-reconcile-*.log). El record de dos `float` usa la forma
`Expand` de Clang, que el plan aun no representa. Estas comprobaciones por si
solas no habilitaban la llamada por valor; el avance posterior figura abajo.

Para alias C vivos, HIR solicita ahora el plan reconciliado `regcall` cuando
la firma contiene records; MIR lo conserva. Una prueba dirigida con records
de ocho y 24 bytes comprueba ambos planes en los dos modelos
(A:/temp/nsc-t09-regcall-record-plan-hir-rjhhle3g); antes del cambio los
planes no aparecian (A:/temp/nsc-t09-regcall-record-plan-hir-8ltxu_et).
Pasaron la prueba de consulta y las regresiones precompiladas SysV y regcall
(A:/temp/t09-regcall-plan-test.log, A:/temp/t09-regcall-hir-reg-*.log).
Esto solo acreditaba el transporte vivo; el resultado posterior del alias
restaurado figura abajo.

## Records C por valor en regcall

Las llamadas que reciben y devuelven el mismo record C por valor se habilitan
cuando el plan de Clang coincide con la representacion fisica Win64 usada por
NSC. La comprobacion contrasta destino, convencion, simbolo, forma fisica,
identidad de la firma y dimensiones; no autoriza una forma expandida solo por
su tamano. Para punteros a funcion exige ademas el plan reconciliado y el
simbolo sintetico esperado.

Una muestra de records de ocho y 24 bytes paso ocho comparaciones C/NativeSharp
en llamadas directas y por puntero, estatica y DLL O0-O3; comprobo copia,
resultado, simbolo, IR y cache (A:/temp/nsc-t09-regcall-records-6cb0maai).
Pasaron los controles de planes reales y alterados
(A:/temp/t09-regcall-plan-test.log). Los casos de dos `float` se rechazaron en
ambas rutas porque su ABI expandido aun no esta representado
(A:/temp/nsc-t09-regcall-hfa-rechazo-s4ychlo6); se mantuvo el rechazo anterior
de `double` por valor (A:/temp/nsc-t09-regcall-negativo-gkcdwerd).

Pasaron cuatro sondas ABI, transporte/identidad y las regresiones
precompiladas SysV y preserve (A:/temp/t09-regcall-records-reg-*.log). En esa
etapa la muestra precompilada fallaba al consumir el plan de una funcion
(A:/temp/nsc-t09-regcall-records-precompilado-2xqyrxyq).
No se acreditan otras firmas ni records expandidos; T09-003 sigue abierto.

## Records C regcall precompilados

El contrato vigente conserva el plan completo de la funcion `regcall` cuando
su simbolo, record y clasificacion coinciden con la regla de emision. El
consumidor reconstruye la declaracion y vuelve a consultar a Clang por el
plan del alias, despues de comprobar identidad, campos y layout del record
del catalogo. Una prueba verifica ida y vuelta y rechazo de convencion,
identidad, dimensiones o datos truncados (A:/temp/t09-regcall-plan-test.log).

Pasaron 16 consumidores precompilados de la muestra de records de ocho y 24
bytes, con y sin encabezado, en estatica y DLL O0-O3
(A:/temp/nsc-t09-regcall-records-precompilado-4fw_4s2h). Pasaron tambien las
regresiones precompiladas SysV, C++ ordinaria y punteros `regcall`, los
rechazos de la forma flotante expandida y las pruebas de metadatos y cierre de
dependencias (A:/temp/t09-regcall-precompiled-reg-*.log y
A:/temp/t09-cpp-meta-check.log).

La evidencia de esta etapa cubre las firmas comprobadas; en ese momento la
representacion y emision de records flotantes expandidos seguian pendientes.
T09-003 no se cierra con este resultado.

## Plan de argumentos regcall expandidos

La sesion obtiene de Clang la clasificacion `Expand`, el tipo fisico y el
desplazamiento en bytes de cada pieza. La correspondencia se comprueba contra
la firma fisica de Clang antes de aceptar el plan. El contrato vigente de
cache conserva esos desplazamientos y rechaza datos truncados o inconsistentes.
La prueba dirigida cubre funciones directas y punteros con dos `float`, un
record anidado y un arreglo de tres `float` (A:/temp/t09-regcall-plan-test.log).
La extraccion no admite uniones, bitfields ni clases con bases en esta ruta.

Pasaron las cuatro sondas ABI/punteros (A:/temp/t09-test-varptr.log),
transporte, ABI e identidad (A:/temp/t09-transport-directed/), comprobaciones
de metadata (A:/temp/t09-cpp-meta-check.log), 16 consumidores regcall
precompilados (A:/temp/nsc-t09-regcall-records-precompilado-lff0py7b) y el
consumidor SysV precompilado (A:/temp/nsc-t09-sysv-indirecta-record-pre-donp4eay).
La prueba existente confirma el rechazo de llamadas flotantes expandidas
directas y por puntero (A:/temp/nsc-t09-regcall-hfa-rechazo-nxjedsgb).
La emision de estas llamadas sigue pendiente; este plan no acredita ejecucion
ni cierra T09-003.

El validador distingue la forma fisica comprobada de dos `float`: retorno
directo en estructura y argumento `Expand` en posiciones 0 y 4. La prueba
consulta planes directos y por puntero, verifica la recuperacion desde cache y
rechaza alteraciones del simbolo, alineacion, desplazamiento, tipo, retorno,
atributos, convencion e indice (A:/temp/t09-regcall-plan-test.log). Esta
validacion no sustituye la comprobacion de identidad del record ni habilita
todavia la emision.

La prueba separada [regcall-hfa](pruebas-tanda-09-regcall-hfa/) compara la
copia, los dos campos del resultado y llamadas directa e indirecta. Sus ocho
controles C con Clang 23.1.0 pasaron en estatica y DLL, O0-O3
(A:/temp/nsc-t09-regcall-hfa-auelvxt7). El primer consumidor NS sigue
rechazado en la barrera de `regcall`
(A:/temp/nsc-t09-regcall-hfa-0hxs8f2d/estatica-O0-nsc.log); no se acredita
ejecucion de NS ni se cierra el item.

## Ejecucion del record de dos flotantes

El plan fisico validado ahora se aplica a la declaracion y a llamadas directas
e indirectas; el argumento se copia en piezas de `float` y el retorno se vuelve
a transportar al record de NS. En el contrato vigente se valida la identidad
del record y se conserva el plan de la funcion; el plan del alias precompilado
se reconstruye y contrasta con Clang. Las ocho comparaciones de la muestra viva
pasaron (A:/temp/nsc-t09-regcall-hfa-96ny3gzx) y los 16 consumidores
precompilados, con y sin encabezado, pasaron
(A:/temp/nsc-t09-hfa-precompilado-yjglukn_). La prueba de planes y metadata,
cuatro sondas ABI, transporte/ABI/identidad, metadata y 16 consumidores de
records enteros tambien pasaron (A:/temp/t09-regcall-plan-test.log,
A:/temp/t09-test-varptr.log, A:/temp/t09-transport-directed/,
A:/temp/t09-cpp-meta-check.log y A:/temp/nsc-t09-regcall-records-precompilado-hctrnjqg).
La comprobacion de LLVM y ejecucion repetida pasa en las ocho configuraciones;
en O0 tambien pasan la cache vacia y su reutilizacion para estatica y DLL
(A:/temp/nsc-t09-regcall-hfa-3862aeck).
La prueba del plan comprueba ademas que la metadata truncada, la identidad
alterada y el tamano incompatible se rechazan (A:/temp/t09-regcall-plan-test.log).

La prueba previa `validar-rechazo-hfa.py` exigia rechazar exactamente la forma
de dos `float` y fallo al habilitarse
(A:/temp/nsc-t09-regcall-hfa-rechazo-1d_606oa/directo/nsc.log). Con autorizacion
del usuario se conserva la prueba y sus dos comprobaciones de rechazo, cambiando
su entrada a un record de tres `float` aun no implementado: ambas pasan
(A:/temp/nsc-t09-regcall-hfa-rechazo-l6_h8phx). La forma de dos `float` queda
comprobada mediante las pruebas de ejecucion descritas arriba. No se acredita
soporte general de records expandidos y T09-003 sigue abierto.

## Sonda C++ de dos flotantes

Una sonda aislada de `T09HfaCpp::Pair` trivial confirma que Clang 23.1.0 usa
`x86_regcallcc`, retorno en estructura y dos argumentos `float`; el control
C++ enlazado y ejecutado devuelve cero. El simbolo observado es
`?transform@T09HfaCpp@@Yw?AUPair@1@U21@@Z`
(A:/temp/t09-cpp-hfa-probe/api.ll y A:/temp/t09-cpp-hfa-probe/symbols.log).
El consumidor NativeSharp todavia no encuentra la funcion en la expresion
(A:/temp/t09-cpp-hfa-probe/nsc.log). Este resultado no acredita consumo C++;
queda pendiente investigar la proyeccion de esa firma y probarla de extremo a
extremo sin extrapolar desde el caso C.

La inspeccion identifica dos barreras que requieren trabajo: la proyeccion de
funciones C++ con registros triviales admite `cdecl` y `preserve_most`, pero
no `regcall` (CppSemanticFunction.cpp); y la consulta de planes de funcion
existente solo acepta sesiones C y busca en el contexto global
(ClangSession.cpp y CppSemanticInventory.cpp). El plan C no se puede
reutilizar para una sobrecarga C++ sin reconciliar nombre calificado, tipo y
simbolo de Clang.

La sesion ahora obtiene el plan de la declaracion C++ exacta en el inventario y
comprueba el simbolo, sin seleccionar por nombre simple. La prueba de planes
cubre espacios de nombres, sobrecarga escalar y un tipo incompleto
(A:/temp/t09-regcall-plan-test.log). Se habilito la proyeccion directa del
record trivial, manteniendo la comprobacion de trivialidad e identidad y el
plan ABI de Clang. La muestra directa pasa en biblioteca estatica y DLL,
O0-O3, contra el control C++ y con inspeccion del LLVM
(A:/temp/nsc-t09-cpp-hfa-1ik7nxag). Pasaron tambien las regresiones de la
firma C de flotantes, su consumo precompilado, `regcall` escalar C++ y record
C++ `cdecl` (A:/temp/nsc-t09-regcall-hfa-5wlx9x12,
A:/temp/nsc-t09-hfa-precompilado-jcjrgecs,
A:/temp/nsc-t09-regcall-cpp-9a7_u_0g y A:/temp/nsc-t09-cpp-record-cdecl-apyq_69r).
En esa etapa la forma C++ por puntero y desde plantillas precompiladas no
quedaba acreditada.
Se intento ademas `nsc.cpp-interop.session`; no ejecuto sus comprobaciones por
falta de `cpp-fixtures/project/CppProbe.nsproj` bajo la raiz configurada
(A:/temp/t09-hfa-session-check.log), limitacion ya registrada en
TANDA-09-VALIDACION.md. No se cuenta esa prueba como aprobada.

## Puntero C++ regcall con record flotante

La consulta viva identifica el alias por nombre calificado, USR y tipo
canonico, y obtiene de Clang un plan de sonda no enlazable. La reconciliacion
exige que el alias y el record C++ trivial procedan de la misma sesion,
mantiene identidad, dimensiones y comprobacion del plan fisico; no autoriza
otros records por semejanza de tamano. La prueba de planes cubre dos espacios
de nombres y rechaza USR, nombre o tipo alterados
(A:/temp/t09-regcall-plan-test.log).

La prueba [cpp-hfa-ptr](pruebas-tanda-09-regcall-cpp-hfa-ptr/) compara llamadas
directas e indirectas con control C++, simbolos, LLVM y valores en estatica y
DLL, O0-O3 (A:/temp/nsc-t09-cpp-hfa-ptr-2zhzdl8j). Pasaron tambien el caso
C precompilado, el C++ directo previo, cuatro sondas ABI, transporte,
identidad y rechazo de tres `float` (A:/temp/nsc-t09-hfa-precompilado-yrorp0dg,
A:/temp/nsc-t09-cpp-hfa-gnrut_zd, A:/temp/t09-test-varptr.log,
A:/temp/t09-transport-directed/ y A:/temp/nsc-t09-regcall-hfa-rechazo-o6j4so_y).
En esa etapa la prueba C++ con plantillas precompiladas aun no estaba acreditada.

## Alias C++ regcall precompilado con record flotante

El productor captura de la sesion de Clang el plan propio del alias y lo
conserva en el contrato NSMeta vigente. Al leerlo se validan firma, identidad,
dimensiones, destino y forma fisica; el consumidor comprueba ademas el record
C++ trivial del catalogo. La prueba de metadata comprueba ida y vuelta,
truncamiento y plan alterado (A:/temp/t09-regcall-plan-test.log).

Pasaron 16 consumidores precompilados con llamada directa y por puntero
(A:/temp/nsc-t09-cpp-hfa-ptr-precompilado-8o96q52s) y otros 16 que solo usan
el puntero (A:/temp/nsc-t09-cpp-hfa-ptr-precompilado-oce164ij), con estatica y
DLL, O0-O3 y con y sin encabezado. La inspeccion del catalogo de solo puntero
confirma el plan en el alias y ausencia de la funcion directa
(A:/temp/t09-cpp-hfa-catalog-inspect.log). Pasaron ademas las pruebas de
metadata, ABI, transporte, identidad, consumidor C precompilado, muestra C++
viva y rechazo de tres `float` (A:/temp/t09-cpp-meta-check.log,
A:/temp/t09-test-varptr.log, A:/temp/t09-transport-directed/,
A:/temp/nsc-t09-hfa-precompilado-37wz_ljz,
A:/temp/nsc-t09-cpp-hfa-ptr-i0o4d6gd y A:/temp/nsc-t09-regcall-hfa-rechazo-xidc2yea).
No se acreditan otros records o firmas; T09-003 sigue abierto.
