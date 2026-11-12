# T09-003: registros flotantes anidados bajo regcall

En Win64, Clang oficial 23.1.0 expande en tres argumentos `float` tanto un
registro formado por otro registro de dos flotantes y un tercer flotante como
un registro que contiene un arreglo fijo de tres flotantes. Sus retornos LLVM
conservan, respectivamente, las formas `{ { float, float }, float }` y
`{ [3 x float] }`. NSC valida las formas, desplazamientos y atributos del
plan de Clang y emite el tipo de retorno correspondiente, tanto para llamadas
directas como a través de punteros.

La prueba dirigida compara un productor y control escritos en C con llamadas
desde NativeSharp. Pasaron las ocho combinaciones de O0 a O3 y enlace estático
o DLL, incluidos valores de retorno y copia del argumento:
`pruebas-tanda-09-regcall-nested/validar.py`, evidencia en
`A:/temp/nsc-t09-regcall-nested-2op99c0t`. La prueba de planes verifica
también el rechazo de retornos y coerciones alterados. Pasaron las regresiones
de registros planos, cuatro pruebas de sondeo ABI y punteros, comprobaciones
de transporte e identidad y 16 consumidores C++ precompilados de registros
planos.

El consumidor precompilado inicialmente falló al reconstruir el alias de
puntero. La reconstrucción ahora recupera sus registros dependientes del
catálogo vigente, comprueba identidad y layout, los ordena según dependencias
y contrasta las formas reconstruidas con Clang. Pasaron los 16 consumidores
con y sin encabezado, estática y DLL, O0-O3, en
`A:/temp/nsc-t09-hfa34-precompilado-6iocixnf`. También pasaron las
regresiones precompiladas de registros planos `regcall`, de punteros
`regcall` y de registros distintos `sysv_abi`, así como los rechazos de
conversiones incompatibles y las pruebas de transporte e identidad.

Las muestras equivalentes de C++ también pasaron las ocho combinaciones de
O0-O3 y estática/DLL, con llamadas directas y por puntero, comparadas con el
control C++ y las formas LLVM de Clang:
`A:/temp/nsc-t09-cpp-nested-3769ljf_`. Pasaron 16 consumidores
precompilados con ambas llamadas y otros 16 que solo llaman por puntero,
con y sin encabezado: `A:/temp/nsc-t09-cpp-nested-precompilado-5qv8j_p3`
y `A:/temp/nsc-t09-cpp-nested-precompilado-ybdszx6k`.

Para que estas muestras sean accesibles, se proyectan los campos públicos
de registros C++ con operaciones de valor elegibles cuando son arreglos
escalares fijos o registros de valor también elegibles, comprobando su
identidad y layout. La prueba negativa conservó el rechazo de campos privados
y del arreglo de un registro con destructor no trivial:
`A:/temp/nsc-t09-cpp-nested-rechazos-1lo8r471`. Pasaron además regresiones
de registros C, registros planos y metadatos C++.

**Pendiente:** otras firmas, layouts y tipos C++ no triviales no están
acreditados. Este avance no cierra T09-003 ni la tanda 9.

## Firma mixta en investigación

Clang 23.1.0 pasa un registro de tres flotantes seguido de `int` como tres
`float` y un `i32`; el plan directo y el plan por puntero registran offsets
0, 4 y 8 y sitúan el entero en el cuarto parámetro físico. El control C del
fixture `pruebas-tanda-09-regcall-mixed` pasó O0-O3 en estática y DLL
(`A:/temp/nsc-t09-regcall-mixed-kmh6vfxp`). NativeSharp todavía rechaza la
firma en HIR (`A:/temp/nsc-t09-regcall-mixed-l2auwyfn/estatica-O0-llvm.log`).

El validador identifica explícitamente este plan mixto y rechaza las
alteraciones de posición, ancho y atributos comprobadas en la prueba de
planes. Las llamadas C directas con los dos registros anidados ya pasan en
O0-O3, estática y DLL, y se comprueban también las llamadas LLVM y la caché:
`A:/temp/nsc-t09-regcall-mixed-jh3_vser`. Las pruebas anteriores de
registros C y C++ siguen pasando; la prueba de rechazo conserva una firma con
dos enteros adicionales que aún no se admite.

La misma muestra C ahora pasa también por puntero en las ocho configuraciones
(`A:/temp/nsc-t09-regcall-mixed-x1kojva9`). Se amplió la validación del
contrato de NSMeta vigente y la reconstrucción del alias para esta firma:
pasaron 16 consumidores precompilados, con y sin encabezado, en O0-O3 con
estática y DLL (`A:/temp/nsc-t09-hfa34-precompilado-aygqfb6a`). La prueba
que solo usa punteros pasó otras 16 combinaciones
(`A:/temp/nsc-t09-hfa34-precompilado-4uw47wo1`). La prueba de metadatos
confirma la lectura del plan y el rechazo de un tipo alterado.
Pasaron las regresiones de transporte, identidad, firmas anteriores y
rechazos conservados.

La muestra C++ equivalente pasó también en las ocho configuraciones con
llamadas directas e indirectas (`A:/temp/nsc-t09-cpp-mixed-o6sglwcs`).
Pasaron 16 consumidores precompilados con ambas llamadas y otros 16 solo por
puntero, con y sin encabezado
(`A:/temp/nsc-t09-cpp-nested-precompilado-bbg_tme6` y
`A:/temp/nsc-t09-cpp-nested-precompilado-hjnt6p5d`). La prueba del contrato
de alias C++ confirma que el plan sobrevive a la serialización y rechaza el
ancho físico alterado. Se comprobaron también los consumidores C mixtos y los
casos C++ previos.

**Pendiente:** otras combinaciones de argumentos y layouts no están acreditadas.

## Varios enteros y presión de registros

La validación, emisión y transporte ahora aceptan una secuencia acotada de
parámetros `int` después del registro expandido, siempre que el plan de
Clang coincida en cada pieza y posición. Una muestra con ocho enteros y pesos
distintos pasó frente a los controles C y C++ en O0-O3, estática y DLL, con
llamadas directas y por puntero:
`A:/temp/nsc-t09-regcall-mixed-y42c97uw` y
`A:/temp/nsc-t09-cpp-mixed-strdcwed`. Pasaron además 64 consumidores
precompilados con y sin encabezado, la mitad de ellos con llamadas únicamente
por puntero (`A:/temp/nsc-t09-hfa34-precompilado-mmth3qaq`,
`A:/temp/nsc-t09-cpp-nested-precompilado-4h5mlib_`,
`A:/temp/nsc-t09-hfa34-precompilado-0kgo2qzv` y
`A:/temp/nsc-t09-cpp-nested-precompilado-b7ls49ec`).

El plan directo y el indirecto de ocho enteros pasaron la prueba de Clang; se
rechazaron posiciones alteradas. También pasaron las regresiones de la firma
de un entero, de registros C++ anteriores y de transporte e identidad. Los
controles negativos conservan ahora una firma con un puntero a función
adicional, que sigue sin admitirse en esta forma.

Se probó también el orden de evaluación NativeSharp usando ocho incrementos
del mismo contador en cada llamada, con valores ponderados por posición y
verificación del contador final. Las llamadas C y C++ directas e indirectas
pasaron O0-O3 en estática y DLL:
`A:/temp/nsc-t09-regcall-mixed-1g_psk67` y
`A:/temp/nsc-t09-cpp-mixed-x60k8r4i`. Pasaron 64 consumidores
precompilados con y sin encabezado, incluidos los que solo llaman por
puntero: `A:/temp/nsc-t09-hfa34-precompilado-xpj82mzw`,
`A:/temp/nsc-t09-cpp-nested-precompilado-rt6jd5hh`,
`A:/temp/nsc-t09-hfa34-precompilado-5r3kmneh` y
`A:/temp/nsc-t09-cpp-nested-precompilado-_jq2pq7c`. Este control cubre
incrementos en argumentos enteros; no acredita todos los efectos laterales ni
las demás clases de argumentos.

## Enteros y flotantes tras el registro

Se generalizó la cola del plan para combinar `int`, `float` y `double`,
verificando cada tipo lógico contra su pieza física y sus atributos antes de
emitir la llamada o aceptar sus metadatos. El control de plan rechaza una
pieza alterada y una declaración de metadatos que no coincide con el plan.

La muestra de registro anidado o arreglo seguida de `int`, `float` y
`double` pasó contra los controles C y C++, directa y por puntero, en O0-O3
con estática y DLL (`A:/temp/nsc-t09-regcall-mixed-8kgoedhv` y
`A:/temp/nsc-t09-cpp-mixed-wtnh94kn`). Pasaron 64 consumidores
precompilados con y sin encabezado, incluyendo la ruta que solo usa punteros
(`A:/temp/nsc-t09-hfa34-precompilado-my2olfmp`,
`A:/temp/nsc-t09-cpp-nested-precompilado-buljcxkb`,
`A:/temp/nsc-t09-hfa34-precompilado-i209b4et` y
`A:/temp/nsc-t09-cpp-nested-precompilado-7iry6fkc`). También pasaron las
regresiones de ocho enteros, transporte e identidad, y los rechazos
conservados. Otras clases de agregados o firmas permanecen pendientes.

## Enteros de 64 bits y puntero

La misma ruta valida y emite una cola con `long long`, `unsigned int`,
`unsigned long long`, `int*` y `double`. El plan y los metadatos deben
coincidir con los tipos y atributos admitidos; las muestras de punteros
concretas se detallan aquí y en la sección siguiente.

Los controles C y C++ y NativeSharp pasaron O0-O3, estática y DLL, en llamadas
directas e indirectas, comprobando valores con bits superiores a 32 y
escrituras por cuatro punteros distintos:
`A:/temp/nsc-t09-regcall-mixed-rniicdne` y
`A:/temp/nsc-t09-cpp-mixed-xi4zkzci`. Pasaron 64 consumidores precompilados
con y sin encabezado, incluidos los que solo usan punteros:
`A:/temp/nsc-t09-hfa34-precompilado-5wz9f9dy`,
`A:/temp/nsc-t09-cpp-nested-precompilado-_qzwcju3`,
`A:/temp/nsc-t09-hfa34-precompilado-eolj40gn` y
`A:/temp/nsc-t09-cpp-nested-precompilado-pi9j6qk1`. El control de plan
rechaza tipos y atributos alterados; pasaron las regresiones de firmas y
metadatos anteriores. Para el caso `const` ver la sección final.

## Enteros estrechos y booleanos

Clang clasifica los enteros de 8 y 16 bits de la muestra como `i8` e
`i16` sin extensión, y el booleano como `i1 zeroext`. NSC conserva el
`zeroext` en el índice físico correcto tanto en la declaración como en la
llamada, después de las piezas expandidas del registro.

La muestra con valores negativos y sin signo pasó con C y C++, directa y por
puntero, O0-O3, estática y DLL (`A:/temp/nsc-t09-regcall-mixed-f1mn_row`
y `A:/temp/nsc-t09-cpp-mixed-l4104f62`). Pasaron 64 consumidores
precompilados con y sin encabezado, incluidos los que solo usan punteros
(`A:/temp/nsc-t09-hfa34-precompilado-l7fvkn4c`,
`A:/temp/nsc-t09-cpp-nested-precompilado-3sv1p927`,
`A:/temp/nsc-t09-hfa34-precompilado-z2if78g7` y
`A:/temp/nsc-t09-cpp-nested-precompilado-rdr4t_29`). La prueba de plan
rechaza retirar `zeroext`; también pasaron los controles negativos y las
regresiones de firmas y metadatos anteriores.

La diferencia observada inicialmente con `unsigned long` quedó resuelta
para el destino y la firma descritos en la sección siguiente. Otras clases
de punteros y formas ABI siguen pendientes.

## Long de 32 bits en Win64

Clang 23.1.0 clasifica `long` y `unsigned long` como `i32` en el destino
Windows x64. La validación ahora los concilia con sus tipos NativeSharp solo
cuando tamaño y alineación observados son de 32 bits; no los confunde con
`long long` de 64 bits. La prueba del contrato rechaza un tamaño alterado.

Las muestras C y C++ usan un `long` negativo y el valor máximo de
`unsigned long`. Pasaron directa y por puntero en O0-O3, estática y DLL
(`A:/temp/nsc-t09-regcall-mixed-ewddjnxe` y
`A:/temp/nsc-t09-cpp-mixed-enzkpfq_`). Pasaron 64 consumidores
precompilados con y sin encabezado, incluidos los que solo usan punteros
(`A:/temp/nsc-t09-hfa34-precompilado-rd8d3ne2`,
`A:/temp/nsc-t09-cpp-nested-precompilado-vedmsz6m`,
`A:/temp/nsc-t09-hfa34-precompilado-6x8o8olm` y
`A:/temp/nsc-t09-cpp-nested-precompilado-x241nt07`). Pasaron también los
rechazos de convenciones y la regresión de enteros estrechos y booleanos.

## Punteros a escalares y void

Se amplió la validación para punteros sin calificadores a escalares admitidos
y `void`, comprobando la identidad del tipo apuntado y su representación.
El camino vivo C ahora coteja también cada tipo con el plan de Clang. La
prueba de plan rechaza un apuntado calificado y una identidad alterada.

Las muestras C y C++ pasan `unsigned char*`, `short*`, `double*` y
`void*`; verifican escrituras, identidad y copia del registro. Pasaron
O0-O3, estática y DLL, directas y por puntero
(`A:/temp/nsc-t09-regcall-mixed-ery0ycn3` y
`A:/temp/nsc-t09-cpp-mixed-1v5k61jt`). Pasaron 64 consumidores
precompilados con y sin encabezado, incluidos los de solo puntero
(`A:/temp/nsc-t09-hfa34-precompilado-zn9ueha_`,
`A:/temp/nsc-t09-cpp-nested-precompilado-gd2v9a8h`,
`A:/temp/nsc-t09-hfa34-precompilado-qobp4hxn` y
`A:/temp/nsc-t09-cpp-nested-precompilado-rjde0sh9`). También pasaron los
controles negativos y las regresiones de transporte.

Los demás calificadores siguen pendientes en esta firma. D23
sigue limitando las direcciones de elementos de arreglos de punteros
calificados y no se acredita con estas muestras.

## Punteros dobles a escalares

La comprobación de tipos ahora recorre cadenas de punteros sin calificadores
con profundidad limitada y conserva la identidad de cada nivel. Una prueba
de plan rechaza un calificador intermedio; no se infiere soporte para esa
forma a partir de un diagnóstico.

Las muestras de `byte**`, `short**` y `double**` pasaron en C y C++,
directas y por puntero, O0-O3, estática y DLL, comprobando escrituras e
identidad de punteros intermedios
(`A:/temp/nsc-t09-regcall-mixed-pdk0vove` y
`A:/temp/nsc-t09-cpp-mixed-tklm0v9s`). Pasaron 64 consumidores
precompilados con y sin encabezado, incluidos los de solo puntero
(`A:/temp/nsc-t09-hfa34-precompilado-s8gntvop`,
`A:/temp/nsc-t09-cpp-nested-precompilado-7cspdw6f`,
`A:/temp/nsc-t09-hfa34-precompilado-09xzha3w` y
`A:/temp/nsc-t09-cpp-nested-precompilado-opw3qou5`). Pasaron controles
negativos y regresiones de punteros simples y transporte. No se acreditan
otros niveles ni calificadores intermedios en esta firma; D23 permanece.

## Puntero a record en la firma expandida

Las muestras pasan un puntero a un record completo, verifican escrituras
sucesivas y conservan la copia del registro flotante. En C++ el tipo apuntado
del alias se proyecta solo cuando su identidad, layout y condición de valor
trivial se verifican. El plan físico no sustituye esa comprobación semántica.

Pasaron los controles y llamadas C y C++ directas e indirectas en O0-O3,
estática y DLL (`A:/temp/nsc-t09-regcall-mixed-s8gsdlyn` y
`A:/temp/nsc-t09-cpp-mixed-yazf_j9m`). Pasaron 64 consumidores
precompilados, incluidos los de solo puntero
(`A:/temp/nsc-t09-hfa34-precompilado-isq0r8dp`,
`A:/temp/nsc-t09-cpp-nested-precompilado-nz846wga`,
`A:/temp/nsc-t09-hfa34-precompilado-jxwxnffl` y
`A:/temp/nsc-t09-cpp-nested-precompilado-v_hkc38h`).

Para reconstruir el alias C precompilado se cotejan el módulo, USR, identidad,
clase de record y layout con el catálogo y el tipo instalado; después se
declara el tipo explícitamente para consultar a Clang. La prueba del plan
rechaza un USR ausente y un nombre alterado. Pasaron los controles negativos y
las regresiones de punteros dobles y transporte, así como los diez rechazos
de tipos C++ no admitidos en la prueba de valor existente
(`A:/temp/nsc-t09-cpp-record-cdecl-rechazos-4d2mua_c`). Esta evidencia no acredita
records incompletos, punteros a records calificados ni records C++ no triviales; D26 y
D23 siguen siendo límites separados.

## Puntero a escalar const

Se admite el calificador `const` en el escalar final apuntado, comprobando
su identidad y representación canónica. La muestra de `const int*` seguida
de `int*` lee el primero y escribe por el segundo. Pasaron las llamadas C y
C++ directas e indirectas frente a sus controles, O0-O3 en estática y DLL:
`A:/temp/nsc-t09-regcall-mixed-tgmx7cs8` y
`A:/temp/nsc-t09-cpp-mixed-52yw23sw`.

Pasaron 64 consumidores precompilados con y sin encabezado, incluidos los
que solo invocan por puntero: `A:/temp/nsc-t09-hfa34-precompilado-3waux3j0`,
`A:/temp/nsc-t09-cpp-nested-precompilado-q5pe70ha`,
`A:/temp/nsc-t09-hfa34-precompilado-aw53th7l` y
`A:/temp/nsc-t09-cpp-nested-precompilado-g5vsu41b`. La prueba de plan
rechaza una identidad de calificador inconsistente. Tres conversiones
incompatibles se rechazaron tanto en C como en C++:
`A:/temp/nsc-t09-regcall-const-rechazos-mxjc61l7` y
`A:/temp/nsc-t09-regcall-const-rechazos-v6rbi0mh`.

Esta evidencia no acredita records calificados, `volatile`,
calificadores intermedios ni todas las expresiones afectadas por D11/D12.
D23 sigue pendiente para arreglos de punteros calificados. T09-003 continúa
abierto.

La validación física rechaza expresamente un record apuntado `const` hasta
verificar su identidad y comportamiento: la prueba de plan construye esa
forma con su nombre canónico y comprueba el rechazo. La compilación
incremental y `nsc.cpp-interop.regcall-record-plan` pasaron.

## Puntero a float const

La clasificación del tipo apuntado usaba el nombre calificado al elegir
entre `float` y `double`, y rechazaba `const float*`. El fallo se reprodujo
en C y C++ (`A:/temp/nsc-t09-regcall-mixed-1fs5rm4s` y
`A:/temp/nsc-t09-cpp-mixed-c8pni1s_`). Ahora usa el nombre canónico sin
calificador para esa elección, conservando las comprobaciones de identidad.

La muestra `const float*` pasó frente a Clang en llamadas directas e
indirectas, O0-O3, estática y DLL en C y C++
(`A:/temp/nsc-t09-regcall-mixed-tw3c85fy` y
`A:/temp/nsc-t09-cpp-mixed-_gw9l295`). Pasaron 64 consumidores
precompilados con y sin encabezado, incluidos los de solo puntero:
`A:/temp/nsc-t09-hfa34-precompilado-rruqdur8`,
`A:/temp/nsc-t09-hfa34-precompilado-ja62hl2m`,
`A:/temp/nsc-t09-cpp-nested-precompilado-zu6vtc1f` y
`A:/temp/nsc-t09-cpp-nested-precompilado-likcotuj`. También pasaron la
regresión precompilada `const int*` y la prueba de plan, que comprueba tanto
`const float*` como `const double*`; la ejecución de esta última se documenta
abajo. Los demás límites de la sección anterior siguen
vigentes y T09-003 permanece abierto.

## Punteros const double y const void

La misma muestra con `const double*` pasó frente a los controles C y C++, en
llamadas directas e indirectas, estática y DLL O0-O3
(`A:/temp/nsc-t09-regcall-mixed-iar6_l2v` y
`A:/temp/nsc-t09-cpp-mixed-lwpd80u1`). Pasaron 64 consumidores
precompilados con y sin encabezado, incluidos los que solo llaman por puntero:
`A:/temp/nsc-t09-hfa34-precompilado-bp0ve361`,
`A:/temp/nsc-t09-hfa34-precompilado-vrkc4io0`,
`A:/temp/nsc-t09-cpp-nested-precompilado-b_yn1tc1` y
`A:/temp/nsc-t09-cpp-nested-precompilado-dwykle1h`.

Las muestras `const void*` se rechazaban en la validación del plan
(`A:/temp/nsc-t09-regcall-mixed-d69lhdak` y
`A:/temp/nsc-t09-cpp-mixed-329v4a4i`). Ahora se admite esa forma cuando su
nombre canónico y calificación concuerdan. La biblioteca C o C++ convierte
el puntero a `const float*` para leer el dato; el consumidor verifica que no
se modificó y que la salida por otro puntero es correcta. Pasaron llamadas
directas e indirectas O0-O3 en estática y DLL
(`A:/temp/nsc-t09-regcall-mixed-tilzzj2i` y
`A:/temp/nsc-t09-cpp-mixed-0_amnwew`), y 64 consumidores precompilados:
`A:/temp/nsc-t09-hfa34-precompilado-10md_jmi`,
`A:/temp/nsc-t09-hfa34-precompilado-alad8l_9`,
`A:/temp/nsc-t09-cpp-nested-precompilado-7m6ydpkn` y
`A:/temp/nsc-t09-cpp-nested-precompilado-g7nmu__w`. La prueba de plan
rechaza un nombre no calificado inconsistente. Esta muestra no acredita todas
las conversiones o accesos a través de `const void*`; D11/D12, D23 y las
restricciones sobre `volatile` y records calificados permanecen.

## Dos registros expandidos: plan en preparación

Clang 23.1.0, en Windows x64, expande cada uno de dos registros anidados de
tres `float` en tres argumentos físicos, seguidos del `int` de la muestra;
el retorno permanece como estructura. El IR directo e indirecto está en
`A:/temp/t09-two-hfa-oracle.ll`. El validador del plan ahora puede comprobar
una secuencia explícita de registros de igual tamaño y alineación; comprueba
cada offset e índice físico. La prueba consulta a Clang tanto para la función
como para el alias de puntero y rechaza un índice, offset o tipo alterado.
Pasó `nsc.cpp-interop.regcall-record-plan`; también pasaron transporte,
identidad, ABI y la regresión `const void*`
(`A:/temp/nsc-t09-regcall-mixed-11fkcyrf`).

La llamada C directa ahora admite varios argumentos expandidos consecutivos
del mismo tipo de registro, seguidos de escalares que coincidan con el plan.
Cada argumento se evalúa en orden y se expande por sus propios offsets. La
muestra de dos registros anidados e `int` pasó frente al control C en enlace
estático O0–O3, con comprobación del LLVM y de que los originales no cambian
(`A:/temp/nsc-t09-two-records-5zlvw5v9`). En esa etapa un segundo record con
otra identidad se rechazaba; el caso está ampliado en la sección siguiente.
La prueba negativa conserva ahora un campo entero con forma ABI incompatible.
Pasaron la prueba de planes,
transporte, identidad y la regresión `const void*` en estática y DLL
(`A:/temp/nsc-t09-regcall-mixed-v8a6ybda`).

La muestra C también pasa por puntero, junto con la llamada directa, en
estática y DLL O0–O3; se inspeccionan ambas llamadas LLVM
(`A:/temp/nsc-t09-two-records-1yscdeln`). El plan del alias se concilia con
Clang y se exige la identidad de cada registro expandido. Pasaron la prueba de
planes, transporte y la regresión de un registro con `const void*`
(`A:/temp/nsc-t09-regcall-mixed-q8mbejyv`).

En el consumo precompilado, el productor que incluye la llamada directa
fallaba al validar el plan y el consumidor que solo usa el puntero no podía
reconstruirlo (`A:/temp/nsc-t09-two-records-pre-yqqiwfvh` y
`A:/temp/nsc-t09-two-records-pre-rvx80v7g`). Se amplió la verificación de la
identidad de cada record y la reconstrucción mediante Clang, manteniendo el
contrato NSMeta vigente. Pasaron 32 consumidores con y sin encabezado, de
O0 a O3, con estática y DLL; la mitad usa solo el puntero
(`A:/temp/nsc-t09-two-records-pre-us91sitv` y
`A:/temp/nsc-t09-two-records-pre-gltbecff`). La prueba de metadatos rechaza
un USR y un offset del segundo record alterados; también pasaron las pruebas
de transporte y la regresión precompilada de `const int*`
(`A:/temp/nsc-t09-hfa34-precompilado-a4z9h61y`).

Las muestras C++ equivalentes usan dos registros del mismo tipo y un entero,
tanto con campos anidados como con arreglo. Pasaron en llamadas directas e
indirectas frente al control C++, con inspección del símbolo y el LLVM, en
estática y DLL O0–O3 (`A:/temp/nsc-t09-cpp-nested-sk3rd92d`). Inicialmente
se rechazaba el alias por puntero (`A:/temp/nsc-t09-cpp-nested-f095hk5y`).

La captura y recuperación del alias C++ se ampliaron con verificación de
identidad de cada record y del plan de Clang. Pasaron 32 consumidores
precompilados con y sin encabezado, incluidos los que solo usan el puntero
(`A:/temp/nsc-t09-cpp-nested-precompilado-vqzojsqn` y
`A:/temp/nsc-t09-cpp-nested-precompilado-d9xih3lc`). Antes de la corrección
se rechazaban ambas variantes (`A:/temp/nsc-t09-cpp-nested-precompilado-ziavevyw`
y `A:/temp/nsc-t09-cpp-nested-precompilado-yi8qibe8`). La prueba del contrato
rechaza un offset alterado del segundo record; también pasaron transporte,
identidad y la regresión precompilada C++ de `const int*`
(`A:/temp/nsc-t09-cpp-nested-precompilado-x1qlwjr7`).
La prueba conservada también rechazó campos privados y el registro con
destructor no trivial (`A:/temp/nsc-t09-cpp-nested-rechazos-xphrvqh3`).

Faltan otras formas. La evidencia de C++ se limita a registros triviales
elegibles. T09-003 permanece pendiente.

## Dos registros C de tipos distintos

La muestra pasa un `Triple` y un `Other` de identidades diferentes, ambos de
12 bytes y tres flotantes, y retorna `Triple`. Clang expande ambos en tres
argumentos físicos cada uno. El control C funcionaba mientras NSC rechazaba
la firma (`A:/temp/nsc-t09-distinct-records-8_ytipqn`).

La selección comprueba cada tipo proyectado y el plan físico; la emisión
utiliza el portador propio de cada argumento y comprueba su tipo declarado.
La llamada C directa pasó frente al control en estática y DLL O0–O3, con
inspección del LLVM (`A:/temp/nsc-t09-distinct-records-ua32i42f`). Pasaron
las pruebas del plan, transporte e identidad y la regresión de dos records
del mismo tipo (`A:/temp/nsc-t09-two-records-_5uqm7hu`). La prueba negativa
de esa regresión ahora usa un segundo record con un campo entero: conserva la
verificación de rechazo de una forma no admitida.

La llamada por puntero y la precompilación inicialmente fallaban
(`A:/temp/nsc-t09-distinct-records-j9xf4j5d` y
`A:/temp/nsc-t09-distinct-records-pre-ma3g02i0`). Ahora pasan las llamadas C
directas e indirectas en estática y DLL O0–O3, con inspección de LLVM
(`A:/temp/nsc-t09-distinct-records-s80aniec`), y 32 consumidores
precompilados con y sin encabezado, incluidos los que solo usan puntero
(`A:/temp/nsc-t09-distinct-records-pre-ub55mds7` y
`A:/temp/nsc-t09-distinct-records-pre-d0p93g2d`). La reconstrucción concilia
cada record con su identidad y el catálogo; una prueba de metadatos rechaza
atribuir al segundo la identidad del primero. Pasaron el plan, transporte y
la regresión precompilada de records iguales
(`A:/temp/nsc-t09-two-records-pre-em6y82v9`).

## Dos registros C++ de tipos distintos

Las muestras C++ emplean un record de retorno y primer argumento, y un
segundo record distinto con igual forma flotante, tanto anidada como de arreglo.
Clang ejecutaba el control y NSC rechazaba el alias de puntero
(`A:/temp/nsc-t09-cpp-distinct-ss1wtqmu`). La conciliación comprueba para
cada record su identidad, layout, procedencia y condición trivial.

Pasaron las llamadas directas e indirectas C++ frente al control, con símbolo
y LLVM comprobados, en estática y DLL O0–O3
(`A:/temp/nsc-t09-cpp-distinct-nlmnwzkn`). Antes de corregir la captura, las
dos rutas precompiladas fallaban
(`A:/temp/nsc-t09-cpp-distinct-precompilado-udiz9l7d` y
`A:/temp/nsc-t09-cpp-distinct-precompilado-5drmwl7x`). Pasaron después 32
consumidores con y sin encabezado, incluidos los que solo usan puntero
(`A:/temp/nsc-t09-cpp-distinct-precompilado-coj3lv44` y
`A:/temp/nsc-t09-cpp-distinct-precompilado-oujgq_2a`). La recuperación
comprueba cada record en el catálogo vigente. Pasaron el control de plan,
transporte, la regresión precompilada de records iguales
(`A:/temp/nsc-t09-cpp-nested-precompilado-uikyl50y`) y los rechazos de
campos privados y record no trivial
(`A:/temp/nsc-t09-cpp-distinct-rechazos-67wjszks`).

No se acreditan aquí records C++ de tamaños distintos, otras formas o
registros C++ no triviales. T09-003 permanece abierto.

## Registros C de tamaños distintos

La muestra usa un record anidado de 12 bytes y otro de 8 bytes. Clang expande
sus argumentos en tres y dos `float`, respectivamente, seguidos de `int`.
El plan directo y el alias de puntero se contrastaron con Clang; el control
rechaza offsets e índices alterados. NSC inicialmente rechazó la llamada
(`A:/temp/nsc-t09-varied-records-v6yvguxb`).

La validación física cuenta las piezas de cada argumento por separado. La
selección coteja tamaño y alineación de cada tipo con su expansión y la
declaración LLVM usa el número real de piezas. La muestra C directa pasó
frente a Clang en estática y DLL O0–O3, con inspección LLVM
(`A:/temp/nsc-t09-varied-records-h1hrecun`). Pasaron la prueba de plan,
transporte, la regresión de registros de igual tamaño
(`A:/temp/nsc-t09-distinct-records-9hmtger0`) y el rechazo de una forma ABI
distinta (`A:/temp/nsc-t09-two-records-v5bt2vtl`).

La ruta por puntero inicialmente fallaba
(`A:/temp/nsc-t09-varied-records-3roehrmt`). Ahora la llamada directa y por
puntero pasa en estática y DLL O0–O3
(`A:/temp/nsc-t09-varied-records-s1txivmp`). Se verifica la identidad y el
tamaño de cada registro contra sus piezas físicas; la reconstrucción también
los concilia con el catálogo. Pasaron 32 consumidores precompilados con y
sin encabezado, incluidos los de solo puntero
(`A:/temp/nsc-t09-varied-records-pre-ri3k0y_b` y
`A:/temp/nsc-t09-varied-records-pre-fdccdlyd`). El control de metadatos
rechaza un tamaño o identidad alterados. Pasaron transporte y la regresión
precompilada con registros de igual tamaño
(`A:/temp/nsc-t09-distinct-records-pre-c7n1_7wg`).

## Registros C++ de tamaños distintos

Las muestras equivalentes usan registros triviales C++ de 12 y 8 bytes,
anidados o con arreglos. El control Clang pasaba y NSC rechazaba el alias
(`A:/temp/nsc-t09-cpp-varied-5vy_gy1a`). La conciliación verifica cada tipo
con su identidad, layout, condición trivial y piezas del plan. Las llamadas
directas e indirectas pasaron en estática y DLL O0–O3, con comparación de
símbolos y LLVM (`A:/temp/nsc-t09-cpp-varied-fz951bsz`).

Las rutas precompiladas inicialmente fallaban
(`A:/temp/nsc-t09-cpp-varied-precompilado-x0ghnid2` y
`A:/temp/nsc-t09-cpp-varied-precompilado-hu_mirfc`). Tras ampliar la captura
y validación de cada argumento, pasaron 32 consumidores con y sin encabezado,
incluidos los que solo usan puntero
(`A:/temp/nsc-t09-cpp-varied-precompilado-4jvz7htc` y
`A:/temp/nsc-t09-cpp-varied-precompilado-udyt0_he`). Una prueba del contrato
rechaza un offset alterado. Pasaron transporte, las restricciones de campos
privados y record no trivial (`A:/temp/nsc-t09-cpp-varied-rechazos-1ul9boay`)
y la regresión precompilada de records de igual tamaño
(`A:/temp/nsc-t09-cpp-distinct-precompilado-ibz8_d5m`).

Otras combinaciones y records C++ no triviales no se acreditan. T09-003 sigue
abierto.

## Primer registro distinto del retorno

Se invirtió el orden de los argumentos de las muestras de 12 y 8 bytes: el
primero tiene 8 bytes y el retorno es un record de 12. En C ya pasaban las
llamadas directas y por puntero O0–O3 con estática y DLL
(`A:/temp/nsc-t09-first-distinct-vffoe9vx`), pero la producción de metadatos
fallaba (`A:/temp/nsc-t09-first-distinct-pre-faqeb0d7`). Se amplió la
validación del contrato para contrastar cada argumento con su propia pieza
del plan. Pasaron 32 consumidores C precompilados con y sin encabezado,
incluidos los que solo usan puntero
(`A:/temp/nsc-t09-first-distinct-pre-70fn33ff` y
`A:/temp/nsc-t09-first-distinct-pre-hgy3kw8t`).

En C++ se rechazaba el alias de puntero
(`A:/temp/nsc-t09-cpp-first-0nakr9vv`). Ahora pasan llamadas directas e
indirectas en estática y DLL O0–O3
(`A:/temp/nsc-t09-cpp-first-1m4wz0it`) y 32 consumidores precompilados
(`A:/temp/nsc-t09-cpp-first-precompilado-jckhuc34` y
`A:/temp/nsc-t09-cpp-first-precompilado-bad7604y`). Las pruebas de metadatos
rechazan tamaños, identidades y offsets alterados; pasaron transporte, la
regresión precompilada C++ anterior
(`A:/temp/nsc-t09-cpp-varied-precompilado-m8usad2a`) y los rechazos de campos
privados y record no trivial (`A:/temp/nsc-t09-cpp-first-rechazos-36fbvw66`).
La evidencia cubre los tipos y firmas descritos; T09-003 sigue abierto.

## Escalar entre registros

La muestra C pasa un record de tres flotantes, un `int` y un record de dos
flotantes. Clang expande las piezas en ese orden; NSC rechazaba inicialmente
la firma (`A:/temp/nsc-t09-interleaved-y8_peyt7`). La validación y emisión
directa recorren ahora el plan en orden, verifican cada tipo lógico y colocan
cada pieza en su posición física.

La llamada directa pasó frente al control C en estática y DLL O0–O3, con
inspección de LLVM (`A:/temp/nsc-t09-interleaved-stsh3_bo`). Pasaron la
prueba del plan y las regresiones de transporte y registros consecutivos
(`A:/temp/nsc-t09-varied-records-um3ipuf_`).

La ruta por puntero inicialmente fallaba
(`A:/temp/nsc-t09-interleaved-gu1sywkl`); ahora pasan la directa y la
indirecta en estática y DLL O0–O3 (`A:/temp/nsc-t09-interleaved-j41ob6r3`).
Las dos rutas precompiladas también fallaban inicialmente
(`A:/temp/nsc-t09-interleaved-pre-eryz43by` y
`A:/temp/nsc-t09-interleaved-pre-njg9xh5u`); pasaron después 32 consumidores
con y sin encabezado, incluidos los que solo usan puntero
(`A:/temp/nsc-t09-interleaved-pre-j1lb8a1k` y
`A:/temp/nsc-t09-interleaved-pre-gwqlq07f`). El control de metadatos rechaza
un tipo o una pieza física alterados. También pasaron transporte y la
regresión precompilada de records consecutivos
(`A:/temp/nsc-t09-varied-records-pre-2u2_lrcr`).

La muestra C++ equivalente inicialmente rechazaba el alias de puntero
(`A:/temp/nsc-t09-cpp-interleaved-alal76mj`). La conciliación verifica cada
record y escalar en la posición indicada por el plan. Pasaron las llamadas
directas e indirectas frente a Clang en estática y DLL O0–O3, con símbolos y
LLVM comprobados (`A:/temp/nsc-t09-cpp-interleaved-q8bam74j`). Pasaron 32
consumidores precompilados con y sin encabezado, incluidos los de solo puntero
(`A:/temp/nsc-t09-cpp-interleaved-precompilado-atizwkhr` y
`A:/temp/nsc-t09-cpp-interleaved-precompilado-mbbko_7z`). El control del
contrato rechaza una pieza escalar alterada; también pasaron transporte, los
rechazos de campos privados y record no trivial
(`A:/temp/nsc-t09-cpp-interleaved-rechazos-9w0km7tu`) y la regresión
precompilada C++ anterior (`A:/temp/nsc-t09-cpp-varied-precompilado-nwvrdj5o`).
La evidencia cubre las firmas y records triviales descritos; otras
combinaciones y T09-003 permanecen pendientes.

## Escalar antes de los registros

Las muestras colocan un `int` antes de dos records flotantes de 12 y 8 bytes.
Las llamadas C y C++ directas e indirectas pasaron frente a sus controles
Clang en estática y DLL O0–O3, con comprobación de LLVM y símbolos
(`A:/temp/nsc-t09-leading-xqeryyxy` y `A:/temp/nsc-t09-cpp-leading-k8nnjn0g`).
También pasaron 32 consumidores C precompilados con y sin encabezado,
incluidos los de solo puntero (`A:/temp/nsc-t09-leading-pre-rhmnwtzw` y
`A:/temp/nsc-t09-leading-pre-27chy22f`).

Los consumidores C++ inicialmente fallaban al recuperar el alias
(`A:/temp/nsc-t09-cpp-leading-precompilado-ixzqgxyz` y
`A:/temp/nsc-t09-cpp-leading-precompilado-ndkev0fk`). La captura del plan ya
no exige un primer argumento de tipo record; mantiene la verificación de cada
argumento y del plan. Pasaron 32 consumidores C++ precompilados
(`A:/temp/nsc-t09-cpp-leading-precompilado-yh8t75bo` y
`A:/temp/nsc-t09-cpp-leading-precompilado-ap_q7bky`). La prueba del contrato
rechaza un escalar físico alterado; pasaron transporte, restricciones de
campos privados y record no trivial
(`A:/temp/nsc-t09-cpp-interleaved-rechazos-lkf7tsn8`) y la regresión
precompilada C++ anterior (`A:/temp/nsc-t09-cpp-interleaved-precompilado-vu5tue8d`).
Otras firmas y T09-003 siguen pendientes.

## Registros expandidos con retorno entero

La muestra C recibe un record de tres flotantes, un entero y otro record de
dos flotantes; devuelve `int` calculado a partir de todas sus entradas.
El control C pasaba y NSC rechazaba la firma
(`A:/temp/nsc-t09-record-ret-int-t1018i3s`). Se amplió la validación y emisión
directa para el retorno `i32`, conservando la comprobación de cada argumento
y su posición física. La muestra pasó frente a C en estática y DLL O0–O3,
con inspección de LLVM (`A:/temp/nsc-t09-record-ret-int-_lwh0npp`).

El control de plan rechaza un retorno y un offset alterados. Pasaron transporte
y la regresión de retorno de record intercalado
(`A:/temp/nsc-t09-interleaved-c4dgqiyk`). La llamada por puntero y la
precompilación inicialmente fallaban (`A:/temp/nsc-t09-record-ret-int-r541rfh9`
y `A:/temp/nsc-t09-record-ret-int-pre-ymczm53g`). Ahora pasan las llamadas C
directas e indirectas en estática y DLL O0–O3
(`A:/temp/nsc-t09-record-ret-int-wigxwcuh`), y 32 consumidores precompilados
con y sin encabezado, incluidos los que solo usan puntero
(`A:/temp/nsc-t09-record-ret-int-pre-eva5c6u4` y
`A:/temp/nsc-t09-record-ret-int-pre-m4l90sxj`). La prueba de metadatos rechaza
un retorno o plan alterados. Pasaron transporte y la regresión precompilada
de retorno de record (`A:/temp/nsc-t09-interleaved-pre-7ozqesj6`).

La muestra C++ equivalente inicialmente rechazaba el alias de puntero
(`A:/temp/nsc-t09-cpp-ret-int-qjkow68n`). La conciliación ahora verifica el
retorno `int` y cada record trivial por separado. Pasaron llamadas directas
e indirectas frente a Clang, con símbolo y LLVM comprobados, en estática y
DLL O0–O3 (`A:/temp/nsc-t09-cpp-ret-int-n59ytgsz`). Las rutas precompiladas
fallaban inicialmente (`A:/temp/nsc-t09-cpp-ret-int-precompilado-1dx77p16` y
`A:/temp/nsc-t09-cpp-ret-int-precompilado-4vf9z0sx`); pasaron después 32
consumidores con y sin encabezado, incluidos los que solo usan puntero
(`A:/temp/nsc-t09-cpp-ret-int-precompilado-ffumazk_` y
`A:/temp/nsc-t09-cpp-ret-int-precompilado-0zbhke_a`). La prueba del contrato
rechaza un retorno físico alterado. Pasaron transporte, las restricciones de
campos privados y record no trivial (`A:/temp/nsc-t09-cpp-ret-int-rechazos-fkwo303r`)
y la regresión precompilada C (`A:/temp/nsc-t09-record-ret-int-pre-r2748dqz`).
Otros retornos y firmas no están acreditados. T09-003 sigue abierto.

## Registros expandidos con retorno double

La muestra C recibe dos records flotantes y un entero intercalado y devuelve
un `double` con parte fraccionaria. El control C pasaba y NSC rechazaba la
firma (`A:/temp/nsc-t09-record-ret-double-fycs44lm`). La validación y emisión
directa ya aceptan el retorno físico `double`; la muestra pasó en estática y
DLL O0–O3 frente a C, con inspección LLVM
(`A:/temp/nsc-t09-record-ret-double-0jkx2eww`). La prueba del plan rechaza un
retorno físico alterado; pasaron transporte y la regresión de retorno `int`
(`A:/temp/nsc-t09-record-ret-int-0jsepp70`).

La llamada por puntero y la precompilación inicialmente fallaban
(`A:/temp/nsc-t09-record-ret-double-y0qbty2m` y
`A:/temp/nsc-t09-record-ret-double-pre-bbuv0zld`). Ahora pasan las llamadas C
directas e indirectas en estática y DLL O0–O3
(`A:/temp/nsc-t09-record-ret-double-wjhsa13b`) y 32 consumidores
precompilados con y sin encabezado, incluidos los que solo usan puntero
(`A:/temp/nsc-t09-record-ret-double-pre-7znfdhql` y
`A:/temp/nsc-t09-record-ret-double-pre-ek4f__el`). La prueba de metadatos
rechaza un retorno alterado; pasaron transporte y la regresión precompilada
de retorno entero (`A:/temp/nsc-t09-record-ret-int-pre-lmgs6eev`).

La muestra C++ equivalente rechazaba el alias de puntero
(`A:/temp/nsc-t09-cpp-ret-double-830wkkjz`). La conciliación ahora verifica
el retorno `double` y los records triviales de cada argumento. Pasaron las
llamadas directas e indirectas frente a Clang en estática y DLL O0–O3, con
símbolos y LLVM comprobados (`A:/temp/nsc-t09-cpp-ret-double-1cf_e85s`).
Pasaron 32 consumidores precompilados con y sin encabezado, incluidos los
que solo usan puntero (`A:/temp/nsc-t09-cpp-ret-double-precompilado-rxr40lk2`
y `A:/temp/nsc-t09-cpp-ret-double-precompilado-db7zn54l`). La prueba del
contrato rechaza un retorno físico alterado; también pasaron transporte,
los rechazos de campos privados y record no trivial
(`A:/temp/nsc-t09-cpp-ret-double-rechazos-9stkg35v`) y la regresión
precompilada C (`A:/temp/nsc-t09-record-ret-double-pre-8ru3amey`).
Otros retornos y firmas no se acreditan. T09-003 permanece abierto.

## Registros expandidos con retorno void

La muestra C recibe dos records flotantes y un entero intercalado y escribe
el resultado mediante `int*`. El control C pasaba y NSC rechazaba la firma
(`A:/temp/nsc-t09-record-ret-void-cvj_cf3l`). Clang clasifica el retorno
lógico como ignorado y el físico como `void`. La validación y emisión directa
comprueban esa forma y los argumentos; pasaron las comparaciones con C en
estática y DLL O0–O3, con inspección LLVM y escritura observable
(`A:/temp/nsc-t09-record-ret-void-kvbtx4ix`).

La prueba del plan rechaza un retorno físico o lógico alterados; pasaron
transporte y la regresión de retorno `double`
(`A:/temp/nsc-t09-record-ret-double-ybc5ubrw`). La llamada C por puntero,
antes rechazada (`A:/temp/nsc-t09-record-ret-void-5t6j9ygg`), también pasó
las ocho comparaciones en estática y DLL O0–O3 con inspección LLVM y escritura
observable (`A:/temp/nsc-t09-record-ret-void-xnk_kn4v`). Se valida el retorno
del plan y se emite la llamada sin asignar un valor a `void`.

La precompilación C, antes rechazada
(`A:/temp/nsc-t09-record-ret-void-pre-c1_p08bc`), pasó 16 variantes con llamada
directa y por puntero (`A:/temp/nsc-t09-record-ret-void-pre-hm53i6xl`) y otras
16 usando solo el puntero (`A:/temp/nsc-t09-record-ret-void-pre-kqjtgr2c`),
con y sin encabezado, estática y DLL, O0–O3. El plan se reconstruye con
retorno `void` y el codec comprueba el tipo y el plan físico y lógico; la
prueba de metadatos acepta el plan válido y rechaza uno alterado y un tipo
calificado. Pasaron transporte, ABI, identidad y la regresión precompilada
de retorno `double` (`A:/temp/nsc-t09-record-ret-double-pre-gwhwrpt3`).

En C++ se probaron records triviales anidados y con arreglos, con dos records,
entero intercalado y puntero de salida. El control C++ pasaba mientras NSC
rechazaba la firma (`A:/temp/nsc-t09-cpp-ret-void-1pdel0ig`): la captura del
plan excluía `void` por ser incompleto para Clang. Se permite ese retorno y se
valida el plan para la llamada por puntero; pasaron ocho variantes de llamada
directa e indirecta, con inspección LLVM y comparación de resultados contra C++
en estática y DLL O0–O3 (`A:/temp/nsc-t09-cpp-ret-void-hiqi59dp`).

Se conservan y validan los planes de alias en metadatos para consumidores
precompilados. Pasaron 16 variantes con ambas llamadas
(`A:/temp/nsc-t09-cpp-ret-void-precompilado-1fofodye`) y 16 solo por puntero
(`A:/temp/nsc-t09-cpp-ret-void-precompilado-klblppg8`), con y sin encabezado,
en estática y DLL O0–O3. Pasaron la regresión precompilada de `double`
(`A:/temp/nsc-t09-cpp-ret-double-precompilado-wue44cjj`), los rechazos de
campos privados y records no triviales
(`A:/temp/nsc-t09-cpp-ret-double-rechazos-p5v1q3cf`), plan, transporte e
identidad. Otras formas no se acreditan. T09-003 permanece abierto.

## Registros expandidos con retorno float

La muestra C combina dos records flotantes y un entero intercalado y devuelve
`float`. El control C pasaba mientras NSC rechazaba la firma
(`A:/temp/nsc-t09-record-ret-float-t1qy9me7`). Se valida el retorno físico y
lógico de Clang y se emite la llamada con retorno LLVM `float`. Las muestras
directa y por puntero pasaron cada una ocho comparaciones en estática y DLL
O0–O3, con inspección LLVM y resultado observable
(`A:/temp/nsc-t09-record-ret-float-y8vyc6u_` y
`A:/temp/nsc-t09-record-ret-float-nbz3sepp`).

Pasaron 16 consumidores precompilados con ambas llamadas
(`A:/temp/nsc-t09-record-ret-float-pre-pdrjahda`) y 16 solo por puntero
(`A:/temp/nsc-t09-record-ret-float-pre-795chcj4`), con y sin encabezado,
estática y DLL O0–O3. La prueba del plan comprueba codificación y rechazo de
un retorno físico alterado; pasaron transporte, ABI, identidad y la regresión
indirecta `void` (`A:/temp/nsc-t09-record-ret-void-u7duqoeb`).

La muestra C++ de records triviales anidados y con arreglos también fue
rechazada inicialmente por NSC (`A:/temp/nsc-t09-cpp-ret-float-fmf8v9uv`).
Con la validación de alias y metadatos ampliada pasaron ocho comparaciones
directas y por puntero, en estática y DLL O0–O3, con inspección LLVM
(`A:/temp/nsc-t09-cpp-ret-float-o3q5ktzi`). Pasaron además 16 consumidores
precompilados con ambas llamadas (`A:/temp/nsc-t09-cpp-ret-float-precompilado-mdrceuh4`)
y 16 solo por puntero (`A:/temp/nsc-t09-cpp-ret-float-precompilado-5fcolumv`),
con y sin encabezado. La regresión precompilada C++ de retorno `void`
(`A:/temp/nsc-t09-cpp-ret-void-precompilado-glk70p02`) y las pruebas de plan,
transporte e identidad pasaron. Otros retornos y firmas siguen pendientes;
T09-003 permanece abierto.

## Registros expandidos con retorno entero de 64 bits

Las muestras C devuelven `long long` y `unsigned long long` tras recibir dos
records flotantes y un entero intercalado. El valor comprobado, 4294967462,
supera 32 bits para detectar truncamiento. El control C firmado pasaba y NSC
rechazaba la firma (`A:/temp/nsc-t09-record-ret-i64-vijyh2p1`). El plan físico
se valida como `i64`, se concilia con el tipo fuente y se conserva al emitir
llamadas y metadatos.

Pasaron ocho comparaciones firmadas directas y ocho por puntero
(`A:/temp/nsc-t09-record-ret-i64-4bmx059t` y
`A:/temp/nsc-t09-record-ret-i64-x58wpxv4`), y ocho sin signo con ambas llamadas
(`A:/temp/nsc-t09-record-ret-u64-nwp2gx1u`), en estática y DLL O0–O3, con
inspección LLVM. Pasaron 16 consumidores precompilados firmados con ambas
llamadas (`A:/temp/nsc-t09-record-ret-i64-pre-pa8vpl83`), 16 solo por puntero
(`A:/temp/nsc-t09-record-ret-i64-pre-5gd68i78`) y 16 sin signo solo por puntero
(`A:/temp/nsc-t09-record-ret-u64-pre-14pugftw`), con y sin encabezado.
La prueba C sin signo se amplió después a 9223372036854775974, con el bit alto
encendido: pasaron ocho comparaciones directas y por puntero
(`A:/temp/nsc-t09-record-ret-u64-9_jth2d9`) y 16 consumidores precompilados
solo por puntero (`A:/temp/nsc-t09-record-ret-u64-pre-c2nuofcx`).

En C++ el control firmado pasaba mientras NSC rechazaba la firma
(`A:/temp/nsc-t09-cpp-ret-i64-pm6gaao8`). Se amplió la conciliación de alias y
metadatos y pasaron ocho comparaciones con y ocho sin signo, ambas directas y
por puntero, estática y DLL O0–O3, con inspección LLVM
(`A:/temp/nsc-t09-cpp-ret-i64-vhbg9sjl` y
`A:/temp/nsc-t09-cpp-ret-u64-55t4xq6x`); la muestra sin signo usa el bit alto.
Pasaron 16 consumidores precompilados de cada combinación de firmado o sin
signo y ambas llamadas o solo puntero, con y sin encabezado:
`A:/temp/nsc-t09-cpp-ret-i64-precompilado-nhxj6_qd`,
`A:/temp/nsc-t09-cpp-ret-i64-precompilado-8000lzsh`,
`A:/temp/nsc-t09-cpp-ret-u64-precompilado-l36d3gtp` y
`A:/temp/nsc-t09-cpp-ret-u64-precompilado-h707qsfk`.
La prueba del plan y el codec rechazan un retorno físico alterado; pasaron
transporte, ABI e identidad. Otros valores y firmas no se acreditan; T09-003
permanece abierto.

## Registros expandidos con retorno de puntero

La muestra C recibe dos records flotantes, un entero y un `int*`, escribe por
ese puntero y devuelve la misma dirección. El control C pasaba mientras NSC
rechazaba la firma (`A:/temp/nsc-t09-record-ret-pointer-xt0kx864`). Se valida
el plan físico `ptr` y se concilia con el tipo fuente `int*`. Pasaron ocho
comparaciones directas y por puntero, estática y DLL O0–O3, comprobando LLVM,
escritura e identidad (`A:/temp/nsc-t09-record-ret-pointer-cgumcx2r`).

Pasaron 16 consumidores precompilados con ambas llamadas
(`A:/temp/nsc-t09-record-ret-pointer-pre-2a09t1ny`) y 16 solo por puntero
(`A:/temp/nsc-t09-record-ret-pointer-pre-evdv7c8v`), con y sin encabezado.
La prueba del plan y el codec aceptan el plan válido y rechazan un retorno
físico alterado; pasaron transporte, ABI e identidad.

La muestra C++ equivalente fue rechazada inicialmente al usar el alias de
función (`A:/temp/nsc-t09-cpp-ret-pointer-38f23pgt`). La conciliación de alias
y metadatos acepta ahora el plan y el tipo `int*` comprobados. Pasaron ocho
comparaciones directas y por puntero con estática y DLL O0–O3, incluyendo
inspección LLVM, escritura e identidad (`A:/temp/nsc-t09-cpp-ret-pointer-p_gi7b43`).
Pasaron 16 consumidores precompilados con ambas llamadas
(`A:/temp/nsc-t09-cpp-ret-pointer-precompilado-e24_hz7k`) y 16 solo por puntero
(`A:/temp/nsc-t09-cpp-ret-pointer-precompilado-jsogb182`), con y sin encabezado.
También pasaron plan, transporte, ABI, identidad y la regresión precompilada
de retorno entero sin signo (`A:/temp/nsc-t09-cpp-ret-u64-precompilado-wjbo9se8`).
Otros tipos de puntero y firmas no se acreditan. T09-003 permanece abierto.

### Retorno de puntero a flotante

Las muestras C y C++ `float*` eran rechazadas por NSC tras pasar sus controles
nativos (`A:/temp/nsc-t09-record-ret-float-pointer-gmvry28h` y
`A:/temp/nsc-t09-cpp-ret-float-pointer-ohatkg2i`). Se amplió la conciliación
de punteros escalares validados contra Clang y se excluyeron punteros a records,
que requieren comprobaciones adicionales de identidad. Pasaron ocho variantes
C y ocho C++ directas y por puntero, con escritura e identidad, estática y DLL
O0–O3 (`A:/temp/nsc-t09-record-ret-float-pointer-et0nxmvi` y
`A:/temp/nsc-t09-cpp-ret-float-pointer-zzw_3_1i`).

Pasaron 16 consumidores precompilados por cada combinación de C o C++ y ambas
llamadas o solo puntero, con y sin encabezado:
`A:/temp/nsc-t09-record-ret-float-pointer-pre-fpfa3czv`,
`A:/temp/nsc-t09-record-ret-float-pointer-pre-da40o_gs`,
`A:/temp/nsc-t09-cpp-ret-float-pointer-precompilado-v9kj3ftj` y
`A:/temp/nsc-t09-cpp-ret-float-pointer-precompilado-irzc2i8r`.
Pasaron plan, transporte e identidad y la regresión precompilada C++ de `int*`
(`A:/temp/nsc-t09-cpp-ret-pointer-precompilado-o_tyr3if`). Los demás tipos de
puntero y firmas requieren pruebas propias; T09-003 sigue abierto.

### Retornos de puntero a short y double

Se contrastaron también retornos `short*` y `double*` con las muestras C y
C++, en llamadas directas y por puntero, con escritura e identidad verificadas.
Cada combinación pasó ocho comparaciones en estática y DLL O0–O3 y 16
consumidores precompilados que usan solo el alias de puntero, con y sin
encabezado. El [validador reutilizable](pruebas-tanda-09-regcall-records-retorno-float-pointer/validar-otros-punteros.py)
genera muestras aisladas a partir de las de `float*`; sus ocho registros están
en `A:/temp/nsc-t09-ret-punteros-escalares-oxk7f_yn`. Otros tipos apuntados,
calificadores y firmas requieren evidencia propia; T09-003 sigue abierto.

## Registros expandidos con retorno entero estrecho

Clang 23.1.0 para Win64 clasificó los retornos de 8 y 16 bits de estas firmas
como `i8` e `i16` sin atributos de extensión; el retorno booleano usa `zeroext`
(`A:/temp/t09-regcall-narrow-oracle.ll`) y se implementó después. En la primera muestra
C el control pasaba y NSC rechazaba la firma (`A:/temp/nsc-t09-record-ret-i8-e9fpikh_`).
Se validan y emiten los retornos estrechos con y sin signo, conciliando el tipo
fuente y el plan físico. La prueba del plan rechaza un ancho o atributo alterado;
pasaron transporte, ABI e identidad.

Para `signed char`, `unsigned char`, `short` y `unsigned short` pasaron en C y
C++ ocho comparaciones por tipo con llamadas directas e indirectas, estática y
DLL O0–O3, e inspección LLVM. Cada combinación pasó además 16 consumidores
precompilados por alias con y sin encabezado. Las muestras usan -90, 166,
-20000 y 50166 para distinguir signo y ancho. Las evidencias C de ejecución son
`A:/temp/nsc-t09-record-ret-i8-87e67jtj`, `A:/temp/nsc-t09-record-ret-u8-yhal9fvq`,
`A:/temp/nsc-t09-record-ret-i16-21oekn9q` y `A:/temp/nsc-t09-record-ret-u16-y37xtuxi`;
las de C++ son `A:/temp/nsc-t09-cpp-ret-i8-ocss092h`, `A:/temp/nsc-t09-cpp-ret-u8-r206jxb5`,
`A:/temp/nsc-t09-cpp-ret-i16-gimyh5l6` y `A:/temp/nsc-t09-cpp-ret-u16-qh57tg24`.

Las evidencias precompiladas C son `A:/temp/nsc-t09-record-ret-i8-pre-e9lq0vbi`,
`A:/temp/nsc-t09-record-ret-u8-pre-4skojlh6`, `A:/temp/nsc-t09-record-ret-i16-pre-rq_42qaz`
y `A:/temp/nsc-t09-record-ret-u16-pre-1far74uh`; las de C++ son
`A:/temp/nsc-t09-cpp-ret-i8-precompilado-3snog7fy`,
`A:/temp/nsc-t09-cpp-ret-u8-precompilado-v0d1nqoe`,
`A:/temp/nsc-t09-cpp-ret-i16-precompilado-z6u8yq6r` y
`A:/temp/nsc-t09-cpp-ret-u16-precompilado-75idkm9g`. El
[validador reutilizable](pruebas-tanda-09-regcall-records-retorno-int/validar-retornos-estrechos.py)
se comprobó para `i8` en ambas lenguas (`A:/temp/nsc-t09-retornos-estrechos-02bsv_uo`).
Otros valores y firmas no se acreditan; T09-003 permanece abierto.

## Registros expandidos con retorno booleano

El control C pasaba y NSC rechazaba la firma
(`A:/temp/nsc-t09-record-ret-bool-q32ld01h`). Clang clasifica el retorno como
`zeroext i1`: NSC comprueba el atributo físico y lógico y lo emite en la
declaración y en llamadas directas e indirectas. Pasaron ocho comparaciones C y
ocho C++ en estática y DLL O0–O3, comprobando el IR y resultados verdadero y
falso (`A:/temp/nsc-t09-record-ret-bool-y2e4uwq1` y
`A:/temp/nsc-t09-cpp-ret-bool-zo7b23sc`).

Pasaron 16 consumidores precompilados para cada combinación de C o C++ y ambas
llamadas o solo puntero, con y sin encabezado:
`A:/temp/nsc-t09-record-ret-bool-pre-b_75el_4`,
`A:/temp/nsc-t09-record-ret-bool-pre-qdlxc05m`,
`A:/temp/nsc-t09-cpp-ret-bool-precompilado-23ga2xom` y
`A:/temp/nsc-t09-cpp-ret-bool-precompilado-m7teu2ni`. La prueba del plan y el
codec rechazan la ausencia del atributo; pasaron transporte, ABI, identidad y
la regresión de retorno entero estrecho (`A:/temp/nsc-t09-retornos-estrechos-t42wqnr8`).
Otras firmas siguen pendientes; T09-003 permanece abierto.

## Registros expandidos con retorno entero de 32 bits sin signo

El control C con `unsigned int` pasaba mientras NSC rechazaba la firma
(`A:/temp/nsc-t09-record-ret-u32-4fwcxz_9`). El plan físico `i32` se concilia
con el tipo fuente sin signo y se conserva en llamadas y metadatos. Pasaron
ocho comparaciones C y ocho C++ directas y por puntero, en estática y DLL
O0–O3, con inspección LLVM y el valor 2147483814, que tiene el bit alto
encendido (`A:/temp/nsc-t09-record-ret-u32-sgljwa4s` y
`A:/temp/nsc-t09-cpp-ret-u32-_x52g3ze`).

Pasaron 16 consumidores precompilados por cada combinación de C o C++ y ambas
llamadas o solo alias, con y sin encabezado:
`A:/temp/nsc-t09-record-ret-u32-pre-_p_ytv2o`,
`A:/temp/nsc-t09-record-ret-u32-pre-ocfay8eq`,
`A:/temp/nsc-t09-cpp-ret-u32-precompilado-vuvxfs4t` y
`A:/temp/nsc-t09-cpp-ret-u32-precompilado-nm2prks4`. Un
[validador adicional](pruebas-tanda-09-regcall-records-retorno-u32/validar-long-win64.py)
contrastó también `long` y `unsigned long` de 32 bits en Win64, C y C++, con
llamadas directas, indirectas y precompiladas (`A:/temp/nsc-t09-ret-long-win64-gfmau448`).
La prueba de metadatos rechaza signo inconsistente; pasaron plan, transporte,
identidad y la regresión booleana (`A:/temp/nsc-t09-record-ret-bool-d7gh9x7m`).
Otras firmas y valores siguen pendientes; T09-003 permanece abierto.

## Registro expandido junto a registro mixto compacto

Clang clasifica un registro con `float` e `int` de ocho bytes como `i64`,
mientras un registro flotante se expande, incluso cuando ambos aparecen en
la misma firma (`A:/temp/t09-regcall-mixed-record-oracle-20261112.ll`). El
control C pasaba pero NSC rechazaba la llamada directa
(`A:/temp/nsc-t09-expanded-mixed-ext5k06w`). Se comprueba la identidad y el
layout observado del registro compacto y se carga su representación `i64`
desde una copia local. Pasaron ocho comparaciones directas en estática y DLL
O0–O3, comprobando LLVM, resultado y que la modificación del parámetro en C
no cambia el original (`A:/temp/nsc-t09-expanded-mixed-xarhl4jz`). Se exige
que la identidad y el layout del registro importado concuerden, y la
elegibilidad trivial para registros C++.

La llamada C por puntero, antes rechazada
(`A:/temp/nsc-t09-expanded-mixed-ec_skqh5`), pasó las ocho comparaciones
en estática y DLL O0–O3, comprobando el plan, resultado y copia
(`A:/temp/nsc-t09-expanded-mixed-9poqmvvd`). La conciliación confirma la
identidad y el layout del registro en su sesión de origen; la emisión usa
una copia local para el portador `i64`. La prueba del plan rechaza un tamaño,
alineación o portador físico alterados. Pasaron transporte, ABI, identidad y
regresión booleana (`A:/temp/nsc-t09-record-ret-bool-9qlq0i5p`).

La precompilación C antes rechazaba el plan de metadatos
(`A:/temp/nsc-t09-expanded-mixed-pre-qz5preuh`). Se conserva la validación del
registro compacto y se reconstruyen ambos registros con sus identidades y
layouts. Pasaron 16 consumidores con ambas llamadas
(`A:/temp/nsc-t09-expanded-mixed-pre-6pf7z6u0`) y 16 solo por puntero
(`A:/temp/nsc-t09-expanded-mixed-pre-3412vdsb`), con y sin encabezado,
estática y DLL O0–O3. La prueba del codec rechaza una alineación alterada;
pasaron transporte, ABI, identidad y la regresión precompilada de `double`
(`A:/temp/nsc-t09-record-ret-double-pre-xyvypmcz`).

La muestra C++ inicialmente fallaba al obtener el plan por alias
(`A:/temp/nsc-t09-cpp-expanded-mixed-4242ea0g`). Se validan todos los records
contra su identidad, layout y elegibilidad trivial, tanto en vivo como desde
el catálogo precompilado. Pasaron ocho comparaciones directas y por puntero en
estática y DLL O0–O3, comprobando resultado y copia
(`A:/temp/nsc-t09-cpp-expanded-mixed-dia3xs3h`). La precompilación, antes
rechazada (`A:/temp/nsc-t09-cpp-expanded-mixed-precompilado-phze164f`), pasó
16 variantes con ambas llamadas (`A:/temp/nsc-t09-cpp-expanded-mixed-precompilado-3h0v9cgx`)
y 16 solo por puntero (`A:/temp/nsc-t09-cpp-expanded-mixed-precompilado-f561r9vb`),
con y sin encabezado. Los controles de acceso a campo privado y de construcción
local no trivial siguen rechazados (`A:/temp/nsc-t09-cpp-expanded-mixed-rechazos-r8o42ttf`).
Pasaron la regresión C precompilada (`A:/temp/nsc-t09-expanded-mixed-pre-xfsf_coe`),
la booleana C++ (`A:/temp/nsc-t09-cpp-ret-bool-precompilado-0o_rkolb`), plan,
transporte e identidad. Otras alineaciones, retornos y layouts no se acreditan;
T09-003 sigue abierto.

### Registros compactos de dos y cuatro bytes

Clang clasifica los records compactos probados de dos y cuatro bytes como
`i16` e `i32` junto al registro flotante expandido
(`A:/temp/t09-regcall-compact-width-oracle-20261112.ll`). La muestra C de
cuatro bytes era rechazada inicialmente (`A:/temp/nsc-t09-expanded-compact4-o0121lt7`).
La validación concilia tamaño, alineación, identidad y ancho físico; la
emisión usa una copia con la alineación del registro. Pasaron ocho comparaciones
directas y por puntero en C y otras ocho en C++, por tamaño, en estática y DLL
O0–O3. También pasaron 16 consumidores precompilados por combinación de lengua,
tamaño y uso de ambas llamadas o solo el alias, con y sin encabezado.

El [validador reutilizable](pruebas-tanda-09-regcall-expanded-mixed-record/validar-tamanos-compactos.py)
reproduce las llamadas y consumidores por alias: registros y resultados de dos
bytes en `A:/temp/nsc-t09-tamanos-compactos-2nk3h5w5`, y de cuatro bytes en
`A:/temp/nsc-t09-tamanos-compactos-73ozddvo`. Los consumidores que usan ambas
llamadas pasaron en `A:/temp/nsc-t09-expanded-compact2-pre-k7czil3g`,
`A:/temp/nsc-t09-cpp-expanded-compact2-precompilado-t4g7x29u`,
`A:/temp/nsc-t09-expanded-compact4-pre-cx6lji_w` y
`A:/temp/nsc-t09-cpp-expanded-compact4-precompilado-86j00_x2`. Pasaron plan,
rechazos de ancho y alineación, transporte e identidad y la regresión C++ de
ocho bytes (`A:/temp/nsc-t09-cpp-expanded-mixed-precompilado-nb7g8sd1`).
Otros tamaños, alineaciones y firmas no se acreditan; T09-003 sigue abierto.

## Registro mixto float y double por copia indirecta

La [muestra C](pruebas-tanda-09-regcall-record-mixto-real/) recibe un registro
de `float` y `double` y retorna `double`. Clang 23.1.0 lo pasa por copia
indirecta, con alineación ocho; inicialmente NSC rechazaba la firma
(`A:/temp/nsc-t09-regcall-record-mixto-real-2h35cztv`). La validación del plan
ahora permite esta forma sin exigir otro registro expandido y comprueba la
identidad y layout del argumento antes de emitir una copia para cada llamada.

El control C y NativeSharp pasaron O0–O3 con llamadas directas y por puntero;
el programa comprueba el retorno y la conservación del original y la prueba
inspecciona la llamada física (`A:/temp/nsc-t09-regcall-record-mixto-real-t0kvo0i_`).
La prueba interna del plan rechaza una alineación alterada; pasaron transporte,
ABI, identidad y la regresión C++ de dos `double` expandidos
(`A:/temp/nsc-t09-cpp-record-double-m67sr12p`). La muestra nueva usa enlace
estático; la muestra C desde fuente con DLL y la variante C++ aún no se
acreditan.

El contrato vigente conserva el plan de copia indirecta sin exigir una pieza
expandida; la prueba interna verifica su lectura y escritura y rechaza una
alineación alterada. Pasaron 16 consumidores precompilados con ambas llamadas y
16 que usan solo el puntero, en estática y DLL O0–O3, con y sin encabezado
(`A:/temp/nsc-t09-record-mixto-real-pre-8nzmvwn5` y
`A:/temp/nsc-t09-record-mixto-real-pre-x21fc31y`). Pasaron transporte, ABI,
identidad y la regresión precompilada C++ de dos `double` expandidos
(`A:/temp/nsc-t09-cpp-record-double-pre-ee8jl2me`).

## Argumento de dos double expandidos: pendiente

La [muestra C](pruebas-tanda-09-regcall-record-double/) pasa un registro de dos
`double` a una función `regcall` y recibe un `double`. Clang 23.1.0 lo expande
en dos argumentos `double`; el control C funciona y NSC aún rechaza la firma
(`A:/temp/nsc-t09-regcall-record-double-l8lwumhm`).

La validación del plan distingue piezas de ocho bytes y comprueba sus
desplazamientos. La prueba interna consultó el plan de Clang y rechazó una
pieza y un desplazamiento alterados. La emisión usa el tipo y la alineación de
cada registro expandido. Pasaron el control C y la ejecución NativeSharp en
O0–O3, directa y por puntero, con inspección de las piezas en LLVM
(`A:/temp/nsc-t09-regcall-record-double-9uikx93_`). También pasaron el plan,
transporte, ABI, identidad y la regresión C++ del retorno indirecto
(`A:/temp/nsc-t09-cpp-return-indirect-align8-f74q3vyb`). La prueba nueva usa
enlace estático desde fuente; la muestra C desde fuente con DLL sigue
pendiente. T09-003 permanece abierto.

El contrato vigente conserva ahora el plan de la función con piezas `double`;
una prueba interna verifica lectura y escritura y rechaza una pieza alterada.
Pasaron 16 consumidores C precompilados con llamadas directa y por puntero y
otros 16 que solo usan el puntero, en estática y DLL O0–O3, con y sin encabezado
(`A:/temp/nsc-t09-record-double-pre-e0htt5zw` y
`A:/temp/nsc-t09-record-double-pre-49_qpqhf`). También pasaron transporte, ABI,
identidad y la regresión C++ precompilada del retorno indirecto
(`A:/temp/nsc-t09-cpp-return-indirect-pre-hiax48jb`).

La [muestra C++](pruebas-tanda-09-regcall-cpp-record-double/) usa un registro
trivial de dos `double` y una función en un espacio de nombres. Pasaron ocho
comparaciones directas y por puntero con Clang, en estática y DLL O0–O3, con
inspección de las piezas LLVM (`A:/temp/nsc-t09-cpp-record-double-ycveqb4l`).
Los consumidores precompilados rechazaban inicialmente el alias por asumir
piezas de `float` (`A:/temp/nsc-t09-cpp-record-double-pre-3neb2bbf`). Tras
conciliar sus piezas con el plan, pasaron 16 consumidores con ambas llamadas y
16 que usan solo el puntero, en estática y DLL, con y sin encabezado
(`A:/temp/nsc-t09-cpp-record-double-pre-f6svlg9d` y
`A:/temp/nsc-t09-cpp-record-double-pre-nnwrx__9`). Pasaron además plan,
transporte, ABI e identidad y los rechazos previos de registros privados y no
triviales (`A:/temp/nsc-t09-cpp-expanded-mixed-rechazos-5nata8vf`). Esta
evidencia no acredita otros registros o firmas.

## Retorno indirecto con argumento flotante expandido: pendiente

La [prueba dirigida](pruebas-tanda-09-regcall-retorno-indirecto/) devuelve un
registro C de seis enteros y recibe un registro de tres flotantes y un entero.
El control C pasó, mientras NSC rechazó la firma
(`A:/temp/nsc-t09-regcall-retorno-indirecto-eyb1ts1v`). El LLVM de Clang
23.1.0 presenta un retorno físico `void`, un parámetro oculto `sret` y tres
argumentos `float` seguidos de un entero.

Se añadió una comprobación del plan que verifica el parámetro oculto, su
alineación y la posición de los argumentos expandidos. La prueba interna
contrastó el plan obtenido de Clang y rechazó un `sret` y un índice alterados;
pasaron compilación incremental y controles de plan, transporte, ABI e
identidad. La ruta directa concilia la identidad y layout del resultado; la
emisión directa y por puntero reserva el resultado y aplica el atributo `sret`.
Pasaron el control C, la ejecución NativeSharp O0–O3 y la inspección LLVM de
ambas llamadas, incluida la alineación del retorno
(`A:/temp/nsc-t09-regcall-retorno-indirecto-_kcxkd6u`). Pasaron también las
regresiones C y C++ del retorno compacto anterior. Esta muestra usa enlace
estático; la muestra C desde fuente con DLL y otros layouts permanecen pendientes.

El contrato vigente de metadatos conserva el retorno indirecto de la función y
el alias de puntero; una prueba interna verifica la lectura y escritura y
rechaza un índice físico alterado. Pasaron 16 consumidores precompilados con
llamadas directas y por puntero y otros 16 que solo usan el puntero, en
estática y DLL O0–O3, con y sin encabezado
(`A:/temp/nsc-t09-return-indirect-pre-ywxtxvqi` y
`A:/temp/nsc-t09-return-indirect-pre-9lu1m0p7`). También pasaron transporte,
ABI, identidad y la regresión precompilada C++ del retorno compacto
(`A:/temp/nsc-t09-cpp-return-compact-pre-kz8fx6od`).

La [muestra C++](pruebas-tanda-09-regcall-cpp-retorno-indirecto/) emplea
registros triviales y una función en un espacio de nombres. Pasaron ocho
comparaciones en estática y DLL O0–O3, con inspección del `sret` en llamadas
directas y por puntero (`A:/temp/nsc-t09-cpp-return-indirect-jkgobl4m`).
También pasaron 16 consumidores precompilados con ambas llamadas y 16 que
usan solo el puntero, en estática y DLL, O0–O3, con y sin encabezado
(`A:/temp/nsc-t09-cpp-return-indirect-pre-_csli2ez` y
`A:/temp/nsc-t09-cpp-return-indirect-pre-lr0qgmup`). Pasaron las pruebas de
plan, transporte, ABI e identidad y los rechazos previos de registros
privados y no triviales (`A:/temp/nsc-t09-cpp-expanded-mixed-rechazos-0g2n3_2f`).
Otras formas y tipos no triviales no se acreditan.

### Retorno C++ indirecto alineado a ocho bytes

La [muestra adicional](pruebas-tanda-09-regcall-cpp-retorno-indirecto-align8/)
retorna un registro trivial de seis enteros de 64 bits, de 48 bytes y
alineación ocho. Antes el alias C++ se rechazaba por exigir alineación cuatro
(`A:/temp/nsc-t09-cpp-return-indirect-align8-35u31wrr`). Ahora se concilia la
alineación observada con el layout importado y con el plan, tanto en la sesión
viva como en el catálogo precompilado. La prueba exige `sret` alineado a ocho.

Pasaron ocho comparaciones en estática y DLL O0–O3, directas y por puntero
(`A:/temp/nsc-t09-cpp-return-indirect-align8-88ekxtv6`), y 32 consumidores
precompilados con ambas llamadas o solo puntero, con y sin encabezado
(`A:/temp/nsc-t09-cpp-return-indirect-align8-pre-m84y7ire` y
`A:/temp/nsc-t09-cpp-return-indirect-align8-pre-v5qf1yeq`). También pasaron
plan, transporte, ABI, identidad, el caso previo alineado a cuatro
(`A:/temp/nsc-t09-cpp-return-indirect-pre-9251l7et`) y los rechazos previos
de registros privados y no triviales
(`A:/temp/nsc-t09-cpp-expanded-mixed-rechazos-lz3v2h_g`). Otros layouts siguen
sin acreditar.

La muestra se reforzó después con dos valores de 64 bits fuera del rango de
32 bits, uno positivo y otro negativo, comprobados en ambas llamadas. Pasaron
de nuevo las ocho comparaciones en estática y DLL
(`A:/temp/nsc-t09-cpp-return-indirect-align8-u246gas8`) y los 32 consumidores
precompilados (`A:/temp/nsc-t09-cpp-return-indirect-align8-pre-h905__7y` y
`A:/temp/nsc-t09-cpp-return-indirect-align8-pre-3ys_fze9`). Esto comprueba esos
valores además de la alineación; no amplía el alcance a otros tipos de retorno.

## Retorno compacto con argumento flotante expandido: pendiente

La [muestra C dirigida](pruebas-tanda-09-regcall-retorno-compacto/) devuelve
un registro de dos enteros y recibe un registro de tres flotantes y un entero.
El control compilado con Clang 23.1.0 ejecutó correctamente las llamadas
directa y por puntero; inicialmente NSC rechazaba la firma
(`A:/temp/nsc-t09-regcall-retorno-compacto-0kb6vpak`). La ruta directa ahora
concilia la identidad y layout del retorno importado, y ambas rutas reconstruyen
el registro a partir del entero físico del plan de Clang. Pasaron el control C,
la ejecución NativeSharp O0–O3 y la comprobación LLVM de las llamadas directa
y por puntero (`A:/temp/nsc-t09-regcall-retorno-compacto-txyj0kta`).
La muestra C desde fuente usa enlace estático; la muestra C desde fuente con
DLL y otros retornos y layouts permanecen pendientes.

El contrato vigente de metadatos conserva ahora este plan para la función y
para el alias de puntero. Una prueba interna verifica su lectura y escritura
y rechaza un retorno físico alterado. Pasaron 16 consumidores precompilados
que usan las llamadas directa y por puntero, y otros 16 que solo usan el
puntero: enlace estático y DLL, O0–O3, con y sin encabezado. Los registros de
ejecución están en `A:/temp/nsc-t09-return-compact-pre-r3rjy70t` y
`A:/temp/nsc-t09-return-compact-pre-eayqygrw`. También pasó la regresión
precompilada C++ anterior con retorno flotante y solo puntero
(`A:/temp/nsc-t09-cpp-return-record-mixed-precompilado-ukjlur17`).

La [muestra C++](pruebas-tanda-09-regcall-cpp-retorno-compacto/) usa tipos
triviales y una función en un espacio de nombres. Después de conservar el
plan del alias C++ en el inventario y reconciliarlo con el tipo importado,
pasaron ocho comparaciones con Clang, en estática y DLL O0–O3, incluidas
las llamadas directas y por puntero y la inspección del LLVM
(`A:/temp/nsc-t09-cpp-return-compact-amu3znns`). Pasaron 16 consumidores
precompilados que usan ambas llamadas y 16 que usan únicamente el puntero,
en estática y DLL, O0–O3 y con y sin encabezado
(`A:/temp/nsc-t09-cpp-return-compact-pre-71zr_rbc` y
`A:/temp/nsc-t09-cpp-return-compact-pre-l0w6deu9`). Pasaron plan, transporte,
ABI e identidad, y el control previo de registros privados y no triviales
(`A:/temp/nsc-t09-cpp-expanded-mixed-rechazos-smioc4ml`). Estos controles no
acreditan otros tamaños, formas ni registros C++ no triviales.

Se añadió una comprobación física compartida para retornos compactos de 1, 2,
4 y 8 bytes junto a argumentos flotantes expandidos. Para el registro de ocho
bytes, la prueba interna contrasta el plan obtenido de Clang y rechaza un
tamaño, una alineación y un tipo físico alterados. La compilación incremental,
la prueba del plan y los controles de transporte, ABI e identidad pasaron.
La comprobación física no sustituye la reconciliación de identidad y layout.
También pasó la regresión previa C++ de retorno flotante en estática y DLL,
O0–O3 (`A:/temp/nsc-t09-cpp-return-record-mixed-w6igwtlj`).

## Retorno de registro con argumento indirecto

La prueba derivada sustituye el argumento compacto por un registro de seis
enteros y 24 bytes, que Clang pasa mediante una copia indirecta. Las muestras
C y C++ comprueban el registro flotante retornado y que el argumento original
no se modifique. En C++ se prueban registros flotantes anidados y con arreglo.
Se contrastaron llamadas directas y por puntero, bibliotecas estáticas y DLL,
O0–O3 y consumidores precompilados directos y por puntero; la muestra de
fuente también verifica la forma física de la llamada en LLVM.

El ejecutable [validar-retorno-registro-grande.py](pruebas-tanda-09-regcall-retorno-record-mixto/validar-retorno-registro-grande.py)
pasó las seis combinaciones de validación con Clang 23.1.0; los comandos y
resultados quedaron en `A:/temp/nsc-t09-retorno-record-grande-57qtc7cg`.
También pasaron la prueba interna del plan `regcall` y los controles de
transporte, ABI e identidad. Esta evidencia corresponde a estas firmas y
layouts; T09-003 y las demás formas aún pendientes permanecen abiertos.

## Retorno de registro flotante con argumento compacto

La muestra C devuelve un record flotante de 12 bytes y recibe otro expandido,
un entero y un record mixto compacto. El control C pasaba mientras NSC
rechazaba la llamada (`A:/temp/nsc-t09-return-record-mixed-12pj999l`). Se
concilian los tipos de retorno y argumento con sus identidades, layouts y
planes; para C++ se exige además elegibilidad trivial. Pasaron ocho
comparaciones directas y por alias, en estática y DLL O0–O3, verificando el
record devuelto y la copia del argumento (`A:/temp/nsc-t09-return-record-mixed-x7woeziq`).

Pasaron 16 consumidores C precompilados con ambas llamadas
(`A:/temp/nsc-t09-return-record-mixed-pre-3u7mcgcb`) y 16 solo por alias
(`A:/temp/nsc-t09-return-record-mixed-pre-43mvfmpc`), con y sin encabezado.
La muestra C++ pasó otras ocho comparaciones
(`A:/temp/nsc-t09-cpp-return-record-mixed-yk4aslv4`) y 32 consumidores
precompilados (`A:/temp/nsc-t09-cpp-return-record-mixed-precompilado-0xa33891`
y `A:/temp/nsc-t09-cpp-return-record-mixed-precompilado-fhcj3myb`). La prueba
del plan y codec rechaza identidades inconsistentes; pasaron transporte,
identidad y las regresiones precompiladas C y C++
(`A:/temp/nsc-t09-expanded-mixed-pre-5o5thweb` y
`A:/temp/nsc-t09-cpp-expanded-large-precompilado-ecencsvr`), además de los
controles existentes de acceso privado y construcción no trivial
(`A:/temp/nsc-t09-cpp-expanded-mixed-rechazos-mq2rt0xv`). Otros argumentos,
retornos y layouts siguen pendientes; T09-003 permanece abierto.

## Registro expandido junto a registro pasado indirectamente

Clang clasifica los registros de tres y 24 bytes observados como punteros a
copias temporales, con sus alineaciones (`A:/temp/t09-regcall-indirect-record-oracle-20261112.ll`).
El control C de 24 bytes pasaba pero NSC rechazaba la firma
(`A:/temp/nsc-t09-expanded-large-4699pxgy`). NSC valida el plan indirecto,
identidad, tamaño y alineación y pasa una copia local. La muestra C directa
pasó ocho comparaciones en estática y DLL O0–O3, con inspección LLVM y
comprobación de que modificar el parámetro no cambia el original
(`A:/temp/nsc-t09-expanded-large-ryuidfu5`).

La llamada por alias y la precompilación C antes fallaban
(`A:/temp/nsc-t09-expanded-large-r9okq2_p` y
`A:/temp/nsc-t09-expanded-large-pre-gtoh9dut`). La conciliación comprueba la
identidad, el layout y el plan indirecto; la emisión pasa una copia con la
alineación validada. Pasaron ocho comparaciones por alias
(`A:/temp/nsc-t09-expanded-large-xzzcp5za`), 16 consumidores precompilados con
ambas llamadas (`A:/temp/nsc-t09-expanded-large-pre-vibacxu4`) y 16 solo por
alias (`A:/temp/nsc-t09-expanded-large-pre-kzksmkpg`), en estática y DLL O0–O3,
con y sin encabezado. Las pruebas del plan y del codec rechazan alineaciones
alteradas; pasaron transporte, ABI, identidad y la regresión precompilada del
registro compacto (`A:/temp/nsc-t09-expanded-mixed-pre-115xd3aw`).

La muestra C++ de records triviales de 24 bytes inicialmente era rechazada
(`A:/temp/nsc-t09-cpp-expanded-large-9zi3i3xn`). La conciliación comprueba el
plan, la identidad, el layout y la elegibilidad trivial en vivo y desde el
catálogo. Pasaron ocho comparaciones directas y por alias en estática y DLL
O0–O3, verificando resultado y copia (`A:/temp/nsc-t09-cpp-expanded-large-hzjpbs5f`).
Pasaron 16 consumidores precompilados con ambas llamadas
(`A:/temp/nsc-t09-cpp-expanded-large-precompilado-mj_yl8jt`) y 16 solo por alias
(`A:/temp/nsc-t09-cpp-expanded-large-precompilado-dg_nlvj7`), con y sin
encabezado. Los controles de acceso privado y construcción local no trivial
siguen rechazados (`A:/temp/nsc-t09-cpp-expanded-large-rechazos-1v3zt7xb`);
pasaron plan, transporte, identidad y la regresión C precompilada
(`A:/temp/nsc-t09-expanded-large-pre-1bb5i1_r`).

También se contrastó un registro de tres bytes y alineación uno junto al
registro expandido. El [validador reutilizable](pruebas-tanda-09-regcall-expanded-large-record/validar-registro-tres-bytes.py)
generó muestras C y C++ aisladas: cada una pasó ocho comparaciones directas
y por alias en estática y DLL O0–O3, comprobando el resultado y la copia, y
16 consumidores precompilados que usan solo el alias, con y sin encabezado
(`A:/temp/nsc-t09-registro-tres-bytes-pbitgr3d`). La prueba del plan rechaza
una alineación física alterada; pasaron transporte, ABI, identidad y la
regresión precompilada C++ de 24 bytes
(`A:/temp/nsc-t09-cpp-expanded-large-precompilado-s8v4sjhv`). Otros layouts y
firmas siguen pendientes. T09-003 permanece abierto.
