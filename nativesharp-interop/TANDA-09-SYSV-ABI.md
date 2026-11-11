# T09-003: primer caso de convencion SysV en Windows x64

Estado: en implementacion; el item T09-003 sigue abierto. Clang oficial 23.1.0
acepta `__attribute__((sysv_abi))` en una biblioteca C para Windows x64. Antes
del cambio, el control C enlazo y ejecuto una suma, mientras NSC rechazo la
convencion en HIR. Reproduccion: A:/temp/nsc-t09-sysv-77xbbf6_.

El primer cambio admite llamadas directas no variadicas con parametros `int`,
`uint`, `long`, `ulong`, `sbyte`, `byte`, `short`, `ushort`, `bool`, `float`,
`double`, `int*`, `double*`, `void*` o `int**` y retorno de esos tipos o `void`.
La convencion se transporta de HIR
a MIR y se aplica en la declaracion y llamada LLVM. Las demas firmas y la
resolucion especial de DLL con alias TLS siguen rechazadas. Convertir la funcion
a delegado queda rechazado hasta representar correctamente ese puntero; no se
confunde este diagnostico con soporte de callbacks SysV.

La prueba `pruebas-tanda-09-sysv/validar.py` compara un productor y consumidor
C con un consumidor NS, en estatica y DLL O0–O3; verifica declaracion y llamada
LLVM y rechazos de firmas y conversiones aun no admitidas. Primera version del
cambio: ocho combinaciones pasaron y se rechazo `double`, evidencia
A:/temp/nsc-t09-sysv-prueba-7ibku3pk. Despues de incorporar la proteccion de
conversion a delegado, la compilacion incremental y las ocho combinaciones
volvieron a pasar, junto con los dos rechazos; evidencia vigente
A:/temp/nsc-t09-sysv-prueba-7oep2i7t. Se incorporo una plantilla NS
precompilada y se oculto su encabezado C: el primer intento fallo por una ruta
equivocada en el fixture, corregida antes de repetir. La ejecucion completa paso,
incluido ese caso en estatica O0, en A:/temp/nsc-t09-sysv-prueba-y_10zhlc.
Las regresiones dirigidas de llamadas,
agregados y vectorcall pasaron 8/8 cada una, respectivamente en
A:/temp/nsc-t09-llamadas-e65389cb, A:/temp/nsc-t09-agregados-fehax39p y
A:/temp/nsc-t09-vectorcall-m9nlyuf7.

Ampliacion de escalares: los controles de `float`, `double` y una firma mixta
con enteros pasaron en estatica y DLL O0–O3. Tambien paso una plantilla NS
precompilada sin encabezado en estatica O0; se verifico el rechazo de un agregado
y de la conversion a callback. Evidencia A:/temp/nsc-t09-sysv-prueba-2d2_0r9n.
Las regresiones de llamadas cdecl y vectorcall pasaron nuevamente 8/8 cada una:
A:/temp/nsc-t09-llamadas-8eb7x7gp y A:/temp/nsc-t09-vectorcall-hga50l6x.

Ampliacion de punteros: se paso y devolvio `int*`, verificando direccion y
escritura desde C y NS. Pasaron estatica y DLL O0–O3 y plantilla precompilada
sin encabezado en estatica O0; tambien se repitieron los rechazos. Evidencia:
A:/temp/nsc-t09-sysv-prueba-aidhp5lh. No se extrapola a otros tipos de puntero.

Ampliacion posterior de punteros: `double*`, `void*` e `int**` conservaron
identidad y, en el caso de `double*`, la modificacion del dato. El control C/NS
paso en estatica y DLL O0–O3 y en la muestra precompilada sin encabezado O0;
evidencia A:/temp/nsc-t09-sysv-prueba-vx7r64fh. No se extrapola a otros tipos
de puntero ni a callbacks.

Ampliacion de enteros: el caso con `uint` de 4.000.000.000, `long` negativo
de -5.000.000.000 y `ulong` de 100 paso en estatica y DLL O0–O3 y en la muestra
precompilada sin encabezado en estatica O0. Los rechazos anteriores siguen
pasando. Evidencia A:/temp/nsc-t09-sysv-prueba-u_11ch2e; los valores probados
no cubren todos los extremos de los tipos.

Enteros estrechos y bool: Clang emite `signext` para signed char y short, y
`zeroext` para unsigned char, unsigned short y `_Bool`. NSC aplica los mismos
atributos en declaracion y llamada SysV. El control automatizado contrasta el
IR de Clang O0 y el de NSC, y ejecuta C y NS en estatica y DLL O0–O3, incluida
una muestra precompilada sin encabezado en estatica O0. Pasaron tambien los
rechazos de agregado y callback; evidencia A:/temp/nsc-t09-sysv-prueba-hkwn48zr.
Las regresiones dirigidas cdecl y vectorcall pasaron 8/8 cada una en
A:/temp/nsc-t09-llamadas-llazaflq y A:/temp/nsc-t09-vectorcall-2vpltaqh.

Investigacion de agregados SysV con Clang 23.1.0 O0, Windows x64:
R3 se pasa y devuelve como `i24`, R4 como `i32`, R8 como `i64`, R16 se
descompone en dos `i64` y R24 usa `sret` y parametro `byval`. Un agregado
mixto de `double` e `int` se descompone en registros de tipos distintos.
Referencia y comando: A:/temp/t09-sysv-records.c, t09-sysv-records.ll y
t09-sysv-records.log. Es una muestra del IR de Clang, no soporte en NSC.
La clasificacion Win64 actual trata R3 indirectamente y no puede reutilizarse
para estos casos; siguen rechazados hasta implementar y comprobar un plan ABI
SysV adecuado para cada forma.

Referencia reproducible: `pruebas-tanda-09-sysv/validar-records-clang.py`
repite las seis clasificaciones y paso en A:/temp/nsc-t09-sysv-records-gtjk7r__.
El modelo Win64 actual expresa paso directo o indirecto y el generador usa un
tipo fisico por parametro; un plan SysV completo tambien necesita representar
descomposicion en varias piezas y clases de registro distintas, `byval` y el
retorno correspondiente. Esta identificacion no habilita esas formas.

Sonda de API oficial: Clang 23.1.0 expone `CodeGenABITypes.h` y
`convertFreeFunctionType`; `CodeGenerator::GetAddrOfGlobal` permite consultar
la declaracion LLVM con su convencion y atributos mientras el generador vive.
El target nuevo `nsc_cpp_abi_probe_tests` crea una unidad C en memoria y verifica
R3 (`i24`), R16 (dos `i64`), un record mixto (`double`, `i32`), R24 (`sret`,
`byval`) y `signed char` (`signext`). La prueba CTest
`nsc.cpp-interop.clang-abi-probe` paso 1/1; registros
A:/temp/t09-abi-probe-build.log y A:/temp/t09-abi-probe-ctest.log.
Esta sonda no integra el plan en NSC ni demuestra que una biblioteca con esos
agregados pueda consumirse; los agregados SysV siguen rechazados. Los objetos
de Clang y LLVM son temporales y no se almacenan en NSMeta ni tras su vida.

La sonda tambien consulta `arrangeFreeFunctionType` y `CGFunctionInfo`: las
muestras R3, R16 y RF tienen retorno y argumento directos, tipo de coercion y
desplazamiento cero; R24 tiene retorno y argumento indirectos, con `byval` y
alineacion 8; signed char usa el plan de extension. La prueba ampliada paso
1/1 con A:/temp/t09-abi-probe-ctest.log. Estos son hechos de las muestras,
no una especificacion completa de todas las clasificaciones o atributos.
El trabajo para transportar ese plan a NSC esta descrito en
[TANDA-09-PLAN-ABI-CLANG.md](TANDA-09-PLAN-ABI-CLANG.md).

Primer agregado implementado: una funcion C SysV no variadica que recibe y
devuelve el mismo record de tres bytes, cuando el plan copiado de Clang verifica
un argumento y un retorno directos `i24` en Windows x64. La prueba dirigida
`pruebas-tanda-09-sysv-record3/validar.py` compara la ejecucion con C, comprueba
la firma LLVM y verifica que la copia original no cambia. Paso para enlace
estatico y DLL en O0–O3: A:/temp/nsc-t09-sysv-r3-wtnwoygk. La sonda del plan
ABI paso 1/1 y sus registros estan en A:/temp/t09-abi-probe-ctest.log. Los
controles de escalares SysV, cdecl y vectorcall pasaron 8/8 cada uno en
A:/temp/nsc-t09-sysv-prueba-7gxaz43g, A:/temp/nsc-t09-llamadas-23_zt50f y
A:/temp/nsc-t09-vectorcall-qgnnxsrk, respectivamente.

La primera plantilla NS precompilada con ese record se rechazo al ocultar el
encabezado porque se perdia el plan ABI. El contrato vigente de dependencias C
ahora conserva la forma acotada `i24` y su destino, sin agregar una estrategia de
versiones historicas. La plantilla paso sin encabezado en estatica O0 en
A:/temp/nsc-t09-sysv-r3-v2gwcf6p; la prueba ahora exige compilacion y ejecucion.
La prueba de identidad y metadatos paso despues de corregir un fixture que habia
heredado indebidamente un atributo de tipo flotante; registros
A:/temp/t09-identidad-build.log y A:/temp/t09-identidad-run.log. Verifica
tambien rechazo de datos truncados y de firmas incompatibles. El control SysV
de escalares y su plantilla previa pasaron en A:/temp/nsc-t09-sysv-prueba-q4eamjec.

La muestra de record ejecutada usa
tres campos `unsigned char`; no acredita todos los layouts o firmas que puedan
tener la misma representacion fisica. Siguen pendientes los demas agregados,
otros escalares o punteros, variadicas, punteros de funcion, aislamiento TLS y
otros destinos. El item T09-003 sigue abierto. No se han cambiado Clang ni NSLink.

Ampliacion a cuatro bytes: el control previo C paso y NSC rechazo la firma en
estatica y DLL O0 (A:/temp/nsc-t09-sysv-r4-2_jbmdt0). El plan de Clang identifica
la forma directa `i32`; NSC ahora la consume cuando argumento y retorno son el
mismo record. El control C/NS paso en estatica y DLL O0–O3 y la plantilla NS
precompilada paso en estatica O0 con encabezado oculto:
A:/temp/nsc-t09-sysv-r4-idoz6jnm, y tras el ajuste final de diagnosticos en
A:/temp/nsc-t09-sysv-r4-wumdw6za. La muestra tiene cuatro campos
`unsigned char`; otras disposiciones no quedan certificadas. Pasaron la sonda
del plan y la prueba de identidad y metadatos, incluidos planes de 24 y 32 bits;
registros A:/temp/t09-abi-probe-ctest.log y A:/temp/t09-identidad-run.log. El
record de tres bytes y escalares SysV volvieron a pasar en
A:/temp/nsc-t09-sysv-r3-w2wk_wy8 y A:/temp/nsc-t09-sysv-prueba-ovfco79r.
Las demas formas SysV siguen abiertas.

Ampliacion agrupada a uno, dos y ocho bytes: Clang emitio respectivamente
`i8`, `i16` e `i64` directos en A:/temp/nsc-t09-sysv-pequenos-_ldgffqv. Los
controles C pasaron y NSC rechazo los tres casos antes de implementarlos:
A:/temp/nsc-t09-sysv-r1-2konpovo, A:/temp/nsc-t09-sysv-r2-tj7jpky8 y
A:/temp/nsc-t09-sysv-r8-_1ak5vlj. Tras el cambio, cada muestra paso las ocho
comparaciones estatica/DLL O0–O3 y la plantilla precompilada sin encabezado
en estatica O0, respectivamente en A:/temp/nsc-t09-sysv-r1-0lijcusy,
A:/temp/nsc-t09-sysv-r2-aptgz5bp y A:/temp/nsc-t09-sysv-r8-1g4nhssk.
Las muestras usan campos `unsigned char` y la misma estructura como argumento
y retorno; no certifican todos los layouts de esos tamanos. El record de tres
bytes paso nuevamente en A:/temp/nsc-t09-sysv-r3-xvpwyzyq y el de cuatro en
A:/temp/nsc-t09-sysv-r4-1a0etas5. Los controles conjuntos del plan de Clang y
de identidad y metadatos pasaron: A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log.

Ampliacion a cinco, seis y siete bytes: Clang reporto `i40`, `i48` e `i56`
directos (A:/temp/nsc-t09-sysv-pequenos-gpe4xtha). Antes del cambio pasaron
los controles C y NSC rechazo cada caso: A:/temp/nsc-t09-sysv-r5-rvqvo404,
A:/temp/nsc-t09-sysv-r6-mdowdwwg y A:/temp/nsc-t09-sysv-r7-ds43mawl. Tras el
cambio pasaron las ocho comparaciones C/NS por tamano en estatica y DLL O0–O3
y la plantilla precompilada sin encabezado en estatica O0: respectivamente
A:/temp/nsc-t09-sysv-r5-eoss30bm, A:/temp/nsc-t09-sysv-r6-sv_3ee0m y
A:/temp/nsc-t09-sysv-r7-28kwqo72. Las muestras usan campos `unsigned char` y
no certifican todos los layouts de esos tamanos. Los casos de tres y ocho bytes
pasaron de nuevo en A:/temp/nsc-t09-sysv-r3-ykdnypg7 y
A:/temp/nsc-t09-sysv-r8-f0edfjzg. La sonda Clang y el control de identidad y
metadatos pasaron, con registros A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. T09-003 sigue abierto para las demas firmas.

Record de 16 bytes: Clang describe la muestra como dos argumentos `i64` y un
retorno `{ i64, i64 }` (A:/temp/nsc-t09-sysv-pequenos-sd7q_mpl); el control C
paso y NSC lo rechazo antes del cambio en A:/temp/nsc-t09-sysv-r16-c2as5kij.
NSC ahora divide y recompone la muestra cuando el plan confirma exactamente
esas dos piezas, conserva esa forma en el contrato vigente de dependencias C
y la rechaza si la evidencia no coincide. Se verificaron declaracion y llamada
LLVM, copia y retorno en ocho comparaciones estatica/DLL O0–O3 y una plantilla
precompilada sin encabezado en estatica O0: A:/temp/nsc-t09-sysv-r16-_cjg9wfo.
La muestra usa dieciseis campos `unsigned char`; no certifica todos los layouts
ni clases de registro. Los controles de tres y ocho bytes y de escalares SysV
pasaron en A:/temp/nsc-t09-sysv-r3-m12vqtcd, A:/temp/nsc-t09-sysv-r8-ccbln1vt y
A:/temp/nsc-t09-sysv-prueba-ij2dqjzw. La sonda del plan y la prueba de metadatos
se registran en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
Los agregados de otra forma, firmas multiples y callbacks siguen pendientes.

Records de nueve a quince bytes: Clang reporto dos piezas, `i64` y un entero
de `i8` a `i56` segun el tamano, en A:/temp/nsc-t09-sysv-pequenos-4ckars9k.
Cada control C paso y NSC lo rechazo antes del cambio. Despues, cada muestra
paso las ocho comparaciones estatica/DLL O0–O3, la verificacion de ambas piezas
en declaracion y llamada, y una plantilla precompilada sin encabezado en
estatica O0. Evidencia por tamano:

- 9: A:/temp/nsc-t09-sysv-r9-nen03wit; 10: A:/temp/nsc-t09-sysv-r10-4l0w99la.
- 11: A:/temp/nsc-t09-sysv-r11-r5rnu6j1; 12: A:/temp/nsc-t09-sysv-r12-ylj2mwer.
- 13: A:/temp/nsc-t09-sysv-r13-5ttz6jxq; 14: A:/temp/nsc-t09-sysv-r14-xbnval2a.
- 15: A:/temp/nsc-t09-sysv-r15-1e2hus4i.

La regresion de 16 bytes paso en A:/temp/nsc-t09-sysv-r16-6p9zi2vs. La prueba
negativa creada en esta sesion conserva el rechazo, ahora con un record de 24
bytes, cuya forma indirecta sigue pendiente; paso junto con los escalares en
A:/temp/nsc-t09-sysv-prueba-_m11qhp6. Pasaron los controles Clang y metadatos
de las siete anchuras (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Las muestras tienen campos `unsigned char`;
otras disposiciones, clases de registro, firmas multiples y callbacks no quedan
certificados. El item sigue abierto.

Record indirecto de 24 bytes: para la muestra de 24 campos `unsigned char`,
Clang emitio retorno `sret` con alineacion 1 y argumento `byval` con alineacion
8 (A:/temp/nsc-t09-sysv-pequenos-k0urrlty). El control C paso y NSC rechazo
el caso previo al cambio en A:/temp/nsc-t09-sysv-r24-czyf6n9a. NSC ahora
reserva una copia de argumento alineada a 8 y conserva el plan acotado en el
contrato vigente. La prueba verifica ambos atributos y alineaciones en
declaracion y llamada, ejecucion C/NS en estatica y DLL O0–O3, y una plantilla
precompilada sin encabezado O0; paso en A:/temp/nsc-t09-sysv-r24-pyqjraer.
La sonda primero encontro que la declaracion de Clang no llevaba `noalias`
aunque la definicion observada si; se corrigio la comprobacion del plan y
pasaron los controles de Clang y metadatos (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log).

Los controles de 3, 9 y 16 bytes pasaron nuevamente en
A:/temp/nsc-t09-sysv-r3-01ogudlm, A:/temp/nsc-t09-sysv-r9-ms9m104o y
A:/temp/nsc-t09-sysv-r16-1rgtxzv0. La prueba negativa incorporada en esta
sesion se conserva con un record de 20 bytes, todavia no admitido; paso junto
con escalares y el rechazo de callbacks en A:/temp/nsc-t09-sysv-prueba-ym6ix5hw.
No se certifican otras alineaciones, layouts, tamanos indirectos ni firmas.

Ampliacion indirecta con alineacion de un byte: Clang confirmo `sret` alineado
a 1 y `byval` alineado a 8 para las muestras de 17, 20 y 32 bytes en
A:/temp/nsc-t09-sysv-pequenos-1vdsqla1. Los controles C pasaron y NSC las
rechazo antes del cambio; posteriormente pasaron las ocho comparaciones por
tamano, los atributos de declaracion y llamada, y una plantilla precompilada
sin encabezado en estatica O0: A:/temp/nsc-t09-sysv-r17-6zkp3kpo,
A:/temp/nsc-t09-sysv-r20-phot1xzy y A:/temp/nsc-t09-sysv-r32-jc6neitb.
La regresion de 24 bytes paso en A:/temp/nsc-t09-sysv-r24-e3ugwimq. La prueba
de 20 bytes alineada a cuatro sigue rechazada, junto con callbacks, en
A:/temp/nsc-t09-sysv-prueba-v0mutgsi. La sonda de Clang y el control de
metadatos, incluido un plan con alineacion incompatible, pasaron en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las muestras
no certifican otros layouts, alineaciones ni firmas; T09-003 permanece abierto.

Ampliacion de alineacion indirecta a cuatro y ocho bytes: el control C paso y
NSC rechazo previamente cada muestra en A:/temp/nsc-t09-sysv-a4-bfp6m1sj y
A:/temp/nsc-t09-sysv-a8-qufzw08w. Tras transportar la alineacion del retorno
observada por Clang, ambos records de 24 bytes pasaron las ocho comparaciones
estatica/DLL O0–O3, atributos en declaracion y llamada y plantilla sin
encabezado en estatica O0, en A:/temp/nsc-t09-sysv-a4-dhw4zjy4 y
A:/temp/nsc-t09-sysv-a8-l39ob5ln. La muestra alineada a cuatro usa seis `int`;
la alineada a ocho usa tres `long long`. La regresion alineada a uno paso en
A:/temp/nsc-t09-sysv-r24-3a3s5_d2. La sonda Clang y el control de metadatos
pasaron (A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log).

La prueba negativa de esta sesion se conserva con un record mixto entero y
`double`, cuya clasificacion fisica aun no esta admitida; paso junto con
escalares y rechazo de callback en A:/temp/nsc-t09-sysv-prueba-7ws2m1_c.
No se certifican otras alineaciones, layouts o firmas.

Primer record mixto: para `struct { double x; int y; }`, Clang utiliza retorno
`{ double, i32 }` y dos argumentos fisicos `double` e `i32`. El primer control C
necesito corregir su propia dependencia `_fltused`; despues paso y NSC rechazo
el caso en A:/temp/nsc-t09-sysv-mixed-di-mfry54ru. Tras el cambio pasaron las
ocho comparaciones estatica/DLL O0–O3, las firmas LLVM y la plantilla sin
encabezado O0: A:/temp/nsc-t09-sysv-mixed-di-2c_tuni2. La sonda Clang y la
prueba de metadatos pasaron (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Volvieron a pasar el record entero de dos
piezas y el indirecto alineado a ocho en A:/temp/nsc-t09-sysv-r16-emtqi600 y
A:/temp/nsc-t09-sysv-a8-y5c0k8se. Una forma mixta distinta sigue rechazada,
junto con callbacks, en A:/temp/nsc-t09-sysv-prueba-7pj53wlf. Esta muestra no
certifica otras combinaciones mixtas ni otras firmas; T09-003 permanece abierto.

Forma mixta inversa: `struct { int x, y; double z; }` usa piezas `i64` y
`double`. El control C paso y NSC rechazo inicialmente la muestra en
A:/temp/nsc-t09-sysv-mixed-id-spgg2ibn. Tras el cambio pasaron las ocho
comparaciones estatica/DLL O0–O3, las firmas LLVM y la plantilla precompilada
sin encabezado O0 en A:/temp/nsc-t09-sysv-mixed-id-7_06db2r. La sonda Clang y
el control de metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Las regresiones de la forma inversa anterior y
del record entero de dos piezas pasaron en A:/temp/nsc-t09-sysv-mixed-di-kdu5s3z0
y A:/temp/nsc-t09-sysv-r16-fg15wbmz. La prueba negativa de esta sesion conserva
el rechazo de `int`, `int`, `float`, junto con el de callback, en
A:/temp/nsc-t09-sysv-prueba-5m4k6kv6. No se certifican otras formas mixtas.

Forma mixta con `float`: `struct { int x, y; float z; }` usa piezas `i64` y
`float` segun Clang (A:/temp/nsc-t09-if-clang-65e4313_). El control C paso y
NSC rechazo inicialmente la muestra en A:/temp/nsc-t09-sysv-mixed-if-a759p778.
Tras el cambio pasaron las ocho comparaciones estatica/DLL O0–O3, las firmas
LLVM y la plantilla sin encabezado O0 en A:/temp/nsc-t09-sysv-mixed-if-qhmqw3ng.
La sonda y el control de metadatos pasaron (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Las regresiones de las formas `int`/`double`
y `double`/`int` pasaron en A:/temp/nsc-t09-sysv-mixed-id-bu3g7hxe y
A:/temp/nsc-t09-sysv-mixed-di-q7c1vh1u. La prueba negativa de esta sesion
conserva una forma con dos `float` que Clang representa mediante vector y aun
se rechaza; paso junto con el rechazo de callbacks en
A:/temp/nsc-t09-sysv-prueba-egocj7n9. No se certifican otras formas mixtas.

Pieza vectorial fija: en `struct { int x, y; float z, w; }`, Clang emite
`{ i64, <2 x float> }` con los mismos dos parametros fisicos
(A:/temp/nsc-t09-if-clang-mbskzzej). El control C paso y NSC rechazo el caso
antes del cambio en A:/temp/nsc-t09-sysv-mixed-iv-l0txw71j. NSC ahora conserva
un vector fijo de dos `float` como tipo ABI distinto de un arreglo, solo para
esta forma. Pasaron las ocho comparaciones estatica/DLL O0–O3, las firmas LLVM
y una plantilla precompilada sin encabezado O0 en
A:/temp/nsc-t09-sysv-mixed-iv-ejc8gq0r. La sonda Clang y el control de
metadatos pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
Las regresiones de las formas con `float` escalar y `double` pasaron en
A:/temp/nsc-t09-sysv-mixed-if-66_d2m89 y A:/temp/nsc-t09-sysv-mixed-di-ltgzjlf4.
La prueba negativa de esta sesion conserva el rechazo de la pieza vectorial
primera y de callback en A:/temp/nsc-t09-sysv-prueba-z3r8ex24. Vectores de
otros tamanos, ordenes y firmas siguen pendientes.

Vector en primera posicion: para `struct { float z, w; int x, y; }`, Clang
produce `{ <2 x float>, i64 }` y parametros en ese orden
(A:/temp/nsc-t09-if-clang-arufezz3). El control C paso y NSC rechazo el caso
antes del cambio en A:/temp/nsc-t09-sysv-mixed-vi-uab1ndmv. Pasaron despues las
ocho comparaciones estatica/DLL O0–O3, las firmas LLVM y una plantilla
precompilada sin encabezado O0 en A:/temp/nsc-t09-sysv-mixed-vi-idnrcomv.
La sonda y metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Las regresiones del orden anterior y de
`int`/`double` pasaron en A:/temp/nsc-t09-sysv-mixed-iv-0qu42spx y
A:/temp/nsc-t09-sysv-mixed-id-w6idklm8. La prueba negativa de esta sesion
conserva el rechazo de un vector primero con pieza `i32` y de callback en
A:/temp/nsc-t09-sysv-prueba-x6h1evqf. Otras anchuras y firmas siguen pendientes.

Vector seguido de `i32`: para `struct { float z, w; short x, y; }`, Clang emite
`{ <2 x float>, i32 }` (A:/temp/nsc-t09-if-clang-xjrjcky4). El control C paso
y NSC rechazo el caso antes del cambio en A:/temp/nsc-t09-sysv-mixed-vs-tr2dok_7.
Despues pasaron las ocho comparaciones estatica/DLL O0–O3, las firmas LLVM y
una plantilla precompilada sin encabezado O0 en A:/temp/nsc-t09-sysv-mixed-vs-d2khp2vh.
La sonda y metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Las regresiones de ambos ordenes con `i64`
pasaron en A:/temp/nsc-t09-sysv-mixed-vi-6udp84_g y
A:/temp/nsc-t09-sysv-mixed-iv-0_xsbob0. La prueba negativa de esta sesion
conserva el rechazo de dos piezas vectoriales y callback en
A:/temp/nsc-t09-sysv-prueba-m1p9hzpj. Otras formas siguen pendientes.

Dos piezas vectoriales: `struct { float x, y, z, w; }` se representa como
dos `<2 x float>` en Clang (A:/temp/nsc-t09-if-clang-0wyfd7a7). El control C
paso y NSC rechazo la muestra antes del cambio en
A:/temp/nsc-t09-sysv-mixed-vv-75stl15b. Despues pasaron las ocho comparaciones
estatica/DLL O0–O3, las firmas LLVM y la plantilla sin encabezado O0 en
A:/temp/nsc-t09-sysv-mixed-vv-l1d0yyot. La sonda y metadatos pasaron en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las regresiones
vector/entero y entero/vector pasaron en A:/temp/nsc-t09-sysv-mixed-vi-bfc4wgmw
y A:/temp/nsc-t09-sysv-mixed-iv-zg39sn9x. La prueba negativa de esta sesion
mantiene el rechazo de tres `float` y de callback en
A:/temp/nsc-t09-sysv-prueba-btryzaxf. Otras formas siguen pendientes.

Vector y escalar: `struct { float x, y, z; }` se representa como
`{ <2 x float>, float }` en Clang (A:/temp/nsc-t09-if-clang-mrzvb9m2). El
control C paso y NSC lo rechazo previamente en A:/temp/nsc-t09-sysv-mixed-vf-7_n8zswb.
Despues pasaron las ocho comparaciones estatica/DLL O0–O3, las firmas LLVM y la
plantilla sin encabezado O0 en A:/temp/nsc-t09-sysv-mixed-vf-qr2gvly1. La sonda
y metadatos pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
Las regresiones de dos vectores y vector/`i32` pasaron en
A:/temp/nsc-t09-sysv-mixed-vv-ekv28p0n y A:/temp/nsc-t09-sysv-mixed-vs-poqmw92g.
La prueba negativa de esta sesion conserva el rechazo de un vector como unica
pieza y de callback en A:/temp/nsc-t09-sysv-prueba-rn4icml4. Otras formas siguen
pendientes.
