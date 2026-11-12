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
