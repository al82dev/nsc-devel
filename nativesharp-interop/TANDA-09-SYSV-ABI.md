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


Vector como unica pieza: `struct { float x, y; }` se transporta como
`<2 x float>` en Clang (A:/temp/nsc-t09-if-clang-dbl6nos2). El control C paso y
NSC lo rechazo antes del cambio en A:/temp/nsc-t09-sysv-mixed-v-ldjdxl0t. Tras
el cambio pasaron las ocho comparaciones estatica/DLL O0–O3, las firmas LLVM y
una plantilla sin encabezado O0 en A:/temp/nsc-t09-sysv-mixed-v-bj39omu6. La
sonda y metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Las regresiones de tres y cuatro `float`
pasaron en A:/temp/nsc-t09-sysv-mixed-vf-7zr2ivrt y
A:/temp/nsc-t09-sysv-mixed-vv-w3ofwo2l. La prueba negativa de esta sesion
conserva el rechazo de un `float` escalar unico y de callback en
A:/temp/nsc-t09-sysv-prueba-bhbu1gfl. Otras formas siguen pendientes.

Records con un escalar flotante: Clang emite un `float` o `double` para las
respectivas muestras (A:/temp/nsc-t09-scalar-clang-t1ozox3m). El control C paso
y NSC rechazo inicialmente los dos casos en A:/temp/nsc-t09-sysv-scalar-f-dsfas6ki
y A:/temp/nsc-t09-sysv-scalar-d-l4y6xkqy. Despues cada muestra paso las ocho
comparaciones estatica/DLL O0–O3, firmas LLVM y plantilla sin encabezado O0:
A:/temp/nsc-t09-sysv-scalar-f-4_kn0bk0 y A:/temp/nsc-t09-sysv-scalar-d-ua6kkpsi.
La sonda y metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. El vector unico paso nuevamente en
A:/temp/nsc-t09-sysv-mixed-v-dmi98swh. La prueba negativa de esta sesion
conserva el rechazo de dos `double` y de callback en
A:/temp/nsc-t09-sysv-prueba-2bzdex5d. Otras formas siguen pendientes.

Dos `double`: Clang emite `{ double, double }` y dos parametros `double`
(A:/temp/nsc-t09-if-clang-95zv0pj0). El control C paso y NSC rechazo la muestra
antes del cambio en A:/temp/nsc-t09-sysv-mixed-dd-k6ycz7bt. Despues pasaron las
ocho comparaciones estatica/DLL O0–O3, las firmas LLVM y la plantilla sin
encabezado O0 en A:/temp/nsc-t09-sysv-mixed-dd-lxwe5f11. La sonda y metadatos
pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las
regresiones `double`/`int` y escalar `double` pasaron en
A:/temp/nsc-t09-sysv-mixed-di-bwd3uxf4 y A:/temp/nsc-t09-sysv-scalar-d-qfch1uvb.
La prueba negativa de esta sesion conserva el rechazo de `double` seguido de
`float` y callback en A:/temp/nsc-t09-sysv-prueba-x75p_hvu. Otras formas siguen
pendientes.

`double` seguido de `float`: Clang usa `{ double, float }`
(A:/temp/nsc-t09-if-clang-lsgq3yea). El control C paso y NSC rechazo la muestra
antes del cambio en A:/temp/nsc-t09-sysv-mixed-df-hziv3z7k. Despues pasaron las
ocho comparaciones estatica/DLL O0–O3, firmas LLVM y plantilla sin encabezado
O0 en A:/temp/nsc-t09-sysv-mixed-df-8ov6c70w. La sonda y metadatos pasaron en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las regresiones
de dos `double` y de `int`/`float` pasaron en
A:/temp/nsc-t09-sysv-mixed-dd-objh6q23 y A:/temp/nsc-t09-sysv-mixed-if-ra2fjiai.
La prueba negativa de esta sesion conserva el rechazo de `float`/`double` y de
callback en A:/temp/nsc-t09-sysv-prueba-p78h10cp. Otras formas siguen pendientes.

`float` seguido de `double`: Clang usa `{ float, double }`
(A:/temp/nsc-t09-if-clang-fban9nfl). El control C paso y NSC rechazo inicialmente
la muestra en A:/temp/nsc-t09-sysv-mixed-fd-wjmno78j. Despues pasaron las ocho
comparaciones estatica/DLL O0–O3, firmas LLVM y plantilla sin encabezado O0 en
A:/temp/nsc-t09-sysv-mixed-fd-7scyydhs. La sonda y metadatos pasaron en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las regresiones
`double`/`float` y dos `double` pasaron en A:/temp/nsc-t09-sysv-mixed-df-q78lo98q
y A:/temp/nsc-t09-sysv-mixed-dd-lx5g54yu. La prueba negativa de esta sesion
conserva el rechazo de vector seguido de `double` y de callback en
A:/temp/nsc-t09-sysv-prueba-oil_chm1. Otras formas siguen pendientes.

Vector y `double` en ambos ordenes: Clang reporto `{ <2 x float>, double }` y
`{ double, <2 x float> }` en A:/temp/nsc-t09-vd-clang-fbjw4t7r. Los controles C
pasaron y NSC los rechazo antes del cambio en A:/temp/nsc-t09-sysv-mixed-vd-wbzrp23r
y A:/temp/nsc-t09-sysv-mixed-dv-707ag_lh. Despues cada muestra paso las ocho
comparaciones estatica/DLL O0–O3, firmas LLVM y plantilla sin encabezado O0 en
A:/temp/nsc-t09-sysv-mixed-vd-ga7r9l_d y A:/temp/nsc-t09-sysv-mixed-dv-a527hom0.
La sonda y metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log; la regresion `float`/`double` paso en
A:/temp/nsc-t09-sysv-mixed-fd-exwn69qi. La prueba negativa de esta sesion
conserva el rechazo de un record alineado a 16 y de callback en
A:/temp/nsc-t09-sysv-prueba-kgfh90kl. Otras formas siguen pendientes.

Alineacion de 16 bytes: el record de cuatro `float` sigue usando dos piezas
vectoriales segun Clang (A:/temp/nsc-t09-if-clang-2jqpqpj5). El control C paso
y NSC rechazo la muestra antes del cambio en A:/temp/nsc-t09-sysv-align16-k3cl06_u.
Despues pasaron las ocho comparaciones estatica/DLL O0–O3, firmas LLVM y
plantilla sin encabezado O0 en A:/temp/nsc-t09-sysv-align16-m58qy1j0. La sonda
y metadatos pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
Las regresiones sin alineacion especial y vector/`double` pasaron en
A:/temp/nsc-t09-sysv-mixed-vv-kf6o1dni y A:/temp/nsc-t09-sysv-mixed-vd-poj6b46t.
La prueba negativa de esta sesion conserva el rechazo de alineacion 32 y de
callback en A:/temp/nsc-t09-sysv-prueba-a0n8us9t. Otras formas siguen pendientes.

Alineacion indirecta de 32 bytes: Clang emitio `sret` y `byval` alineados a 32
(A:/temp/nsc-t09-if-clang-g4xgitu7). El control C paso y NSC rechazo el caso
antes del cambio en A:/temp/nsc-t09-sysv-align32-z8vnq7qm. Despues pasaron las
ocho comparaciones estatica/DLL O0–O3, los atributos LLVM y una plantilla sin
encabezado O0 en A:/temp/nsc-t09-sysv-align32-hn4edud2. La sonda y metadatos
pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las
regresiones de 16 y 8 bytes pasaron en A:/temp/nsc-t09-sysv-align16-dd2ikeld y
A:/temp/nsc-t09-sysv-a8-jcw607ff. La prueba negativa de esta sesion conserva
el rechazo de un record indirecto alineado a dos y de callback en
A:/temp/nsc-t09-sysv-prueba-2cj5q4xx. Otras formas siguen pendientes.

Alineacion indirecta de dos bytes: Clang emitio `sret` alineado a 2 y `byval`
alineado a 8 (A:/temp/nsc-t09-if-clang-oduvo4ld). El control C paso y NSC
rechazo el caso antes del cambio en A:/temp/nsc-t09-sysv-a2-sl04vxyt. Despues
pasaron las ocho comparaciones estatica/DLL O0–O3, atributos LLVM y plantilla
sin encabezado O0 en A:/temp/nsc-t09-sysv-a2-wj1w5xxp. La sonda y metadatos
pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las
regresiones de 4 y 32 bytes pasaron en A:/temp/nsc-t09-sysv-a4-pj75qy41 y
A:/temp/nsc-t09-sysv-align32-a8m2_n2s. La prueba negativa de esta sesion
conserva el rechazo de un record indirecto alineado a 16 y de callback en
A:/temp/nsc-t09-sysv-prueba-tff82wku. Otras formas siguen pendientes.

Alineacion indirecta de 16 bytes: Clang emitio `sret` y `byval` alineados a 16
(A:/temp/nsc-t09-if-clang-2qu6jyx9). El control C paso y NSC rechazo el caso
antes del cambio en A:/temp/nsc-t09-sysv-align16-ind-lvz8f6_i. Despues pasaron
las ocho comparaciones estatica/DLL O0–O3, los atributos LLVM y la plantilla
sin encabezado O0 en A:/temp/nsc-t09-sysv-align16-ind-6xjgmpmn. La sonda y
metadatos pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
Las regresiones directo a 16 e indirecto a 32 pasaron en
A:/temp/nsc-t09-sysv-align16-d4vw6mv3 y A:/temp/nsc-t09-sysv-align32-w1q9pn0p.
La prueba negativa de esta sesion conserva el rechazo de alineacion indirecta
64 y de callback en A:/temp/nsc-t09-sysv-prueba-gcqjvlza. Otras formas siguen
pendientes.

Alineaciones indirectas mayores: Clang emitio `sret` y `byval` alineados a 64,
128 y 256 en A:/temp/nsc-t09-highalign-clang-jioufv_3. El primer control C no
enlazo porque el fixture aislado carecia de `memcpy` y `memset`; se incorporo
un soporte minimo al propio fixture y los controles pasaron, reproduciendo el
rechazo previo de NSC. Tras conservar la alineacion completa del plan en el
contrato vigente, cada muestra paso las ocho comparaciones estatica/DLL O0–O3,
atributos LLVM y plantilla sin encabezado O0:
A:/temp/nsc-t09-sysv-align64-o1ke9jvx,
A:/temp/nsc-t09-sysv-align128-o8hykaac y
A:/temp/nsc-t09-sysv-align256-1pbc9cil. La sonda y metadatos pasaron en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. La regresion de
32 bytes paso en A:/temp/nsc-t09-sysv-align32-0qe7h9zw. La prueba negativa de
esta sesion conserva el rechazo de dos argumentos de record y de callback en
A:/temp/nsc-t09-sysv-prueba-qx2bfuqv. Otras firmas siguen pendientes.

Dos argumentos de record pequeno: Clang emitio `i32 (i32, i32)` para la
muestra de cuatro bytes (A:/temp/nsc-t09-if-clang-u2420j88). El control C paso
y NSC rechazo la muestra antes del cambio en A:/temp/nsc-t09-sysv-two-ffh3pq_x.
Despues pasaron las ocho comparaciones estatica/DLL O0–O3, declaracion y llamada
LLVM, orden de argumentos, copias y plantilla sin encabezado O0 en
A:/temp/nsc-t09-sysv-two-7ajd8cz_. La sonda y metadatos pasaron en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Las regresiones
de un record y dos vectores pasaron en A:/temp/nsc-t09-sysv-r4-4qhuf7p6 y
A:/temp/nsc-t09-sysv-mixed-vv-3jyp565l. La prueba negativa de esta sesion
conserva el rechazo de tres records y de callback en
A:/temp/nsc-t09-sysv-prueba-8lqn7jbp. Otras firmas siguen pendientes.

Varios argumentos de record pequeno: Clang emitio ocho argumentos `i32` para
la muestra de cuatro bytes (A:/temp/nsc-t09-if-clang-0pu9xuh8). El control C
paso y NSC rechazo inicialmente la firma en A:/temp/nsc-t09-sysv-eight-1l4naxr7.
Se generalizo la forma directa del mismo tipo para varios argumentos. La muestra
de ocho, que supera los registros de argumentos disponibles, paso las ocho
comparaciones estatica/DLL O0–O3, firmas LLVM, orden, copias y plantilla sin
encabezado O0 en A:/temp/nsc-t09-sysv-eight-ajv5p1ja. La sonda y metadatos
pasaron en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
Las regresiones de dos y un record pasaron en A:/temp/nsc-t09-sysv-two-beru2_iy
y A:/temp/nsc-t09-sysv-r4-6wjag0eb. La prueba negativa de esta sesion conserva
el rechazo de records de tipos distintos y de callback en
A:/temp/nsc-t09-sysv-prueba-xe3d6crd. Otras firmas siguen pendientes.
# Records pequenos de distintos tipos

Clang clasifico la muestra con argumentos de cuatro y ocho bytes como `i32` e
`i64`, con retorno `i32` (A:/temp/nsc-t09-if-clang-bq434o1n). Antes del
cambio paso el control C y NSC rechazo la firma
(A:/temp/nsc-t09-sysv-hetero-kl2q518l). NSC ahora comprueba y usa la pieza
fisica de cada argumento y conserva el plan admitido en los metadatos vigentes.

Tras la compilacion incremental, la prueba dirigida comparo C y NativeSharp en
estatica y DLL, de O0 a O3, y ejecuto una plantilla precompilada sin encabezado
en estatica O0: A:/temp/nsc-t09-sysv-hetero-fclt0rpk. La sonda de Clang
comprueba tambien el rechazo de un plan inconsistente y la prueba de identidad
verifica la conservacion del plan: A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Pasaron las regresiones de dos y ocho records
(A:/temp/nsc-t09-sysv-two-_qj_vbgq y A:/temp/nsc-t09-sysv-eight-kpzhsqik)
y la prueba general, incluidos los rechazos pendientes
(A:/temp/nsc-t09-sysv-prueba-58j5cssi).

La muestra no acredita todos los layouts ni firmas arbitrarias con records y escalares,
varios records indirectos, callbacks u otras clasificaciones. T09-003 permanece
abierto.

## Record pequeno seguido de `int`

El control C paso y NSC rechazo inicialmente esta firma en
A:/temp/nsc-t09-sysv-record-scalar-gutfsm6a. Se amplio el consumo del plan
directo para una firma de dos argumentos: record pequeno seguido de `int` de
32 bits, con retorno de record pequeno. La muestra de cuatro bytes compara
copias y resultado C/NS, comprueba la declaracion y llamada LLVM, y paso en
estatica y DLL O0–O3 y mediante plantilla precompilada sin encabezado en O0:
A:/temp/nsc-t09-sysv-record-scalar-hd1m4vq9. La sonda de Clang y la prueba de
metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. La regresion general y sus rechazos pasaron
en A:/temp/nsc-t09-sysv-prueba-znbfi5om; los records de tipos distintos
pasaron en A:/temp/nsc-t09-sysv-hetero-ci0h0vp_. Otras posiciones, tipos de
escalar y clasificaciones permanecen pendientes; T09-003 sigue abierto.

El orden inverso, `int` seguido del record, tenia controles C correctos y NSC
lo rechazaba (A:/temp/nsc-t09-sysv-scalar-record-j8b999pu). Se amplio el
consumo del plan para ese orden, siempre con dos argumentos y al menos un
record. La muestra paso las ocho comparaciones estatica/DLL O0–O3 y la
plantilla precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-scalar-record-7d9j9daf. La regresion del orden anterior
paso en A:/temp/nsc-t09-sysv-record-scalar-yljdztd2 y las pruebas de plan y
metadatos en A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
En ese paso otras mezclas, cantidades de argumentos y tipos de escalar seguian pendientes.

Posteriormente se probo una firma de ocho argumentos que intercala records de
cuatro bytes e `int`, incluidos argumentos en pila. El control C paso y NSC
rechazo inicialmente el caso (A:/temp/nsc-t09-sysv-mixed-n1d_958c); tras
ampliar el plan, pasaron las comparaciones estatica/DLL O0–O3 y la plantilla
precompilada sin encabezado O0 (A:/temp/nsc-t09-sysv-mixed-bxz8rw3f).
La regresion de dos argumentos paso en
A:/temp/nsc-t09-sysv-scalar-record-_w55bx67; la sonda Clang y el control de
metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Siguen pendientes otros tipos escalares y
otras clasificaciones de records; no se acredita compatibilidad general.

## Record pequeno seguido de `float`

El control C paso y NSC rechazo inicialmente la firma en estatica y DLL O0
(A:/temp/nsc-t09-sysv-record-float-v5r37_1c). Tras usar la clasificacion de
Clang para el parametro flotante, la muestra paso ocho comparaciones C/NS en
estatica y DLL O0–O3 y una plantilla precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-record-float-i3wd0v2u. La sonda Clang y el control del
plan en metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Las regresiones de ocho argumentos mixtos y
del control general pasaron en A:/temp/nsc-t09-sysv-mixed-ptb5sy4y y
A:/temp/nsc-t09-sysv-prueba-wf6mqllu. Para el control C aislado se definio
`_fltused` localmente; no se modifico NSLink. Se comprobaron un record de cuatro
bytes y el `float` en segundo lugar; otras firmas flotantes siguen pendientes.

La variante con `double` tenia controles C correctos y rechazo previo de NSC
(A:/temp/nsc-t09-sysv-record-double-0oxy_6u1). Tras la ampliacion paso ocho
comparaciones C/NS estatica/DLL O0–O3 y la plantilla precompilada sin
encabezado O0 (A:/temp/nsc-t09-sysv-record-double-qepvwywt). La regresion de
`float` paso en A:/temp/nsc-t09-sysv-record-float-juy36erc y la general, con
rechazos pendientes, en A:/temp/nsc-t09-sysv-prueba-bncej3ey. Sonda Clang y
metadatos: A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
La muestra conserva el record de cuatro bytes y el escalar en segundo lugar;
otras posiciones y mezclas flotantes siguen pendientes.

El orden `float` seguido de record tenia controles C correctos y rechazo previo
de NSC (A:/temp/nsc-t09-sysv-float-record-oysiibi8). Tras la ampliacion paso
estatica y DLL O0–O3 y la plantilla precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-float-record-u08i08t0. La regresion de record y `double`
paso en A:/temp/nsc-t09-sysv-record-double-_g230mg9 y el control general con
rechazos pendientes en A:/temp/nsc-t09-sysv-prueba-hd3y_0hi. La sonda Clang y
metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. El orden inverso para `double` y otras mezclas
flotantes siguen pendientes.

La variante `double` seguido de record fue rechazada por NSC antes del cambio,
con controles C correctos (A:/temp/nsc-t09-sysv-double-record-f_1s1tk5).
Despues paso las ocho comparaciones estatica/DLL O0–O3 y la plantilla
precompilada sin encabezado O0 (A:/temp/nsc-t09-sysv-double-record-_o6y_mvl).
La regresion `float` primero paso en A:/temp/nsc-t09-sysv-float-record-g8nkh7br
y el control general con rechazo de tres argumentos aun no soportados en
A:/temp/nsc-t09-sysv-prueba-358_aru0. Sonda Clang y metadatos:
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Los casos
probados usan dos argumentos y un record de cuatro bytes; firmas flotantes
con mas argumentos u otras clasificaciones siguen pendientes.

Se amplio despues a tres argumentos directos con la muestra `float`, record de
cuatro bytes, `float`. Antes del cambio el control C paso y NSC rechazo la
firma (A:/temp/nsc-t09-sysv-floats-record-sswgin5t); despues pasaron las ocho
comparaciones estatica/DLL O0–O3 y plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-floats-record-6a9ae422). La regresion de `double` antes
del record paso en A:/temp/nsc-t09-sysv-double-record-s_e6izqm; el control
general y el rechazo de cuatro argumentos flotantes pendientes pasaron en
A:/temp/nsc-t09-sysv-prueba-vu1tz324. La sonda Clang y metadatos pasaron en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log. Siguen
pendientes otras mezclas, clasificaciones y casos con mas argumentos.

## Argumentos flotantes directos y pila

La firma de control con un record de cuatro bytes y nueve escalares `float` y
`double` alternados supera los ocho registros flotantes SysV. Antes del cambio
el control C paso en estatica y DLL y NSC rechazo la firma
(A:/temp/nsc-t09-sysv-float-stack-m1krm5v0). Se eliminaron los limites de
cantidad para las piezas directas de esos dos tipos que el plan de Clang
describe sin atributos adicionales. La muestra paso ocho comparaciones C/NS
estatica/DLL O0–O3, comprobacion de la firma LLVM y una plantilla precompilada
sin encabezado O0 (A:/temp/nsc-t09-sysv-float-stack-6h2w1mig). La sonda de
Clang y el control de metadatos tambien incluyeron diez argumentos
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron las
regresiones de tres argumentos (A:/temp/nsc-t09-sysv-floats-record-g_s1fgey)
y del control general, incluido el rechazo de un record de varias piezas en
una firma mixta (A:/temp/nsc-t09-sysv-prueba-0pgj78jv).

Esto no acredita combinaciones con records que necesiten multiples piezas,
argumentos indirectos, punteros u otros escalares en la misma firma, ni todas
las disposiciones de registros. T09-003 sigue abierto.

## Record pequeno con puntero a entero

El control C de la firma con record de cuatro bytes e `int*` paso en estatica
y DLL, mientras NSC la rechazo antes del cambio
(A:/temp/nsc-t09-sysv-record-pointer-1m0dhm__). El plan ahora admite el tipo
fisico de puntero del espacio de direcciones cero para esta forma, y HIR permite
el tipo `int*` comprobado. La muestra verifico copia y retorno del record,
lectura y modificacion del entero apuntado, declaracion y llamada LLVM, ocho
comparaciones estatica/DLL O0–O3 y plantilla precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-record-pointer-4hvlkehu. La sonda Clang verifica ademas
el rechazo de otro espacio de direcciones y la prueba de metadatos paso en
A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log; la regresion de
flotantes en pila paso en A:/temp/nsc-t09-sysv-float-stack-puxxn49a. Otros
punteros y clasificaciones de records en firmas mixtas siguen pendientes.

## Record de dos piezas enteras seguido de escalares

Una firma con record de doce bytes, `int` y `float` paso los controles C y fue
rechazada inicialmente por NSC (A:/temp/nsc-t09-sysv-pair-mix-wwdrr860). El
primer intento de soporte obtuvo un rechazo del verificador LLVM porque MIR
seguia clasificando el retorno como indirecto
(A:/temp/nsc-t09-sysv-pair-mix-ty_2qfv4); se corrigio MIR para conservar la
clasificacion y posiciones fisicas del plan de Clang. El generador emite dos
piezas del record seguidas de los escalares y reconstruye el retorno, y el
contrato vigente conserva la forma validada del plan.

La muestra de doce bytes paso ocho comparaciones C/NS estatica/DLL O0–O3,
incluido el orden de evaluacion de argumentos nombrados, y una plantilla
precompilada sin encabezado O0 con argumentos nombrados
(A:/temp/nsc-t09-sysv-pair-mix-g_ltc7ay). La muestra de dieciseis bytes paso
las mismas ocho combinaciones y precompilacion O0
(A:/temp/nsc-t09-sysv-pair16-mix-mgasai0v). La sonda Clang comprueba la
clasificacion y rechaza posiciones fisicas inconsistentes; la prueba de
metadatos comprueba la reconstruccion del plan
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron la
regresion de doce bytes sin escalares (A:/temp/nsc-t09-sysv-r12-7knjzh53) y
el control general con sus rechazos (A:/temp/nsc-t09-sysv-prueba-f8jskb5m).

Las muestras comprueban un record primero, con retorno del mismo tipo, seguido
de `int` y `float`; no acreditan todos los layouts, otros ordenes ni records de
clases flotantes o indirectos en firmas mixtas. T09-003 permanece abierto.

## Enteros de 32 y 64 bits con records

La firma de record de doce bytes seguido de `uint`, `long` y `ulong` tenia
controles C correctos y era rechazada por NSC
(A:/temp/nsc-t09-sysv-pair-ints-jaq1p27z). La ampliacion verifica el tipo y
anchura de cada escalar frente al plan de Clang y conserva esas anchuras en
el contrato vigente. Dos muestras, de record de cuatro y doce bytes, pasaron
las ocho comparaciones estatica/DLL O0–O3 y la plantilla precompilada sin
encabezado O0: A:/temp/nsc-t09-sysv-small-ints-2z5_42f5 y
A:/temp/nsc-t09-sysv-pair-ints-chj_3v26. El productor usa las partes altas
de un entero negativo y otro sin signo de 64 bits para detectar truncamiento.
La sonda Clang y metadatos pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log, y la regresion del record con `int`/`float`
en A:/temp/nsc-t09-sysv-pair-mix-0zjk9uli. Otros enteros y clasificaciones
siguen pendientes; T09-003 no se cierra.

## Varios records de dos piezas enteras

El control C con dos records de doce bytes y un `int` paso mientras NSC
rechazaba la firma (A:/temp/nsc-t09-sysv-two-pairs-nk0guwpj). Se amplio el
consumo del plan para varios records del mismo tipo, siempre que Clang
clasifique cada uno en las dos piezas enteras verificadas; las posiciones
fisicas se conservan para el generador y el contrato vigente de metadatos.

La muestra de dos records paso las ocho comparaciones estatica/DLL O0–O3,
llamadas con argumentos nombrados reordenados y plantilla precompilada sin
encabezado O0 (A:/temp/nsc-t09-sysv-two-pairs-iamulhsl). Una muestra de tres
records iguales y un `int` agota los seis registros enteros SysV y pasa el
entero restante por pila; supero las mismas comprobaciones, incluida la
precompilacion (A:/temp/nsc-t09-sysv-three-pairs-e3p7ep85). La sonda de Clang
verifico la distribucion fisica y la prueba de metadatos la reconstruccion
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron las
regresiones del record con `int`/`float` y del record con enteros de 64 bits
(A:/temp/nsc-t09-sysv-pair-mix-r8b7b1v4 y
A:/temp/nsc-t09-sysv-pair-ints-xh7dixch).

Las muestras no acreditan records de diferentes tipos o clases fisicas, ni
records que Clang pase indirectamente cuando faltan registros. T09-003 sigue
abierto.

Se amplio despues a records de distintos tipos y tamanos, siempre que Clang
los clasifique en dos piezas enteras. La muestra de records de doce y dieciseis
bytes y un `int` tenia controles C correctos y NSC la rechazaba antes del
cambio (A:/temp/nsc-t09-sysv-different-pairs-1ki7irrc). El plan, el generador
y el contrato vigente conservan por separado la anchura de cada pieza. La
muestra paso ocho comparaciones estatica/DLL O0–O3, argumentos nombrados
reordenados y plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-different-pairs-puq2r3_t). La sonda Clang y el control
de metadatos verificaron las anchuras diferentes
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log); pasaron las
regresiones de tres records iguales y del record con `int`/`float`
(A:/temp/nsc-t09-sysv-three-pairs-4aosay6u y
A:/temp/nsc-t09-sysv-pair-mix-k34m9kq8). No se acreditan records indirectos,
otros tipos de pieza ni todos los layouts; T09-003 permanece abierto.

Se probo despues la misma mezcla con el `int` antes de ambos records. El
control C paso y NSC rechazo inicialmente la firma
(A:/temp/nsc-t09-sysv-scalar-pairs-seh9pj3u). La emision ahora recorre el
orden logico y las posiciones fisicas descritas por Clang, y el contrato
vigente conserva ese orden. La muestra paso estatica y DLL O0–O3, argumentos
nombrados reordenados y plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-scalar-pairs-cz3j048p). La sonda Clang y la prueba de
metadatos verificaron las posiciones (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Pasaron las regresiones de records de
distinto tamano, tres records, piezas `double`/entero y dos piezas vectoriales
(A:/temp/nsc-t09-sysv-different-pairs-_t2pp3wq,
A:/temp/nsc-t09-sysv-three-pairs-s_9a8158,
A:/temp/nsc-t09-sysv-mixed-di-0z_u0aen y
A:/temp/nsc-t09-sysv-mixed-vv-75bvrj_f), y el control general con rechazo
de un record indirecto mixto (A:/temp/nsc-t09-sysv-prueba-bgf8v0mk). Quedan
pendientes otras clases y formas indirectas; T09-003 continua abierto.

## Records de una y dos piezas en la misma firma

La referencia C con un record de doce bytes, otro de cuatro e `int` paso, y
NSC rechazaba esa firma (A:/temp/nsc-t09-sysv-pair-small-6650stbj). Se
ampliaron la validacion, conservacion del plan y generacion para distinguir
un record de una pieza de un entero escalar con el mismo tipo fisico. La
muestra paso ocho comparaciones estatica/DLL O0–O3, argumentos nombrados y
plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-pair-small-lw338m45); el orden inverso paso las mismas
comprobaciones (A:/temp/nsc-t09-sysv-small-pair-stnk29ds). Sonda Clang y
metadatos: A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log.
Pasaron las regresiones de escalar seguido de dos records y de dos piezas
vectoriales (A:/temp/nsc-t09-sysv-scalar-pairs-ghzfuzky y
A:/temp/nsc-t09-sysv-mixed-vv-6g1w9lix). La muestra usa records de cuatro y
doce bytes con clasificacion entera; no certifica otras clases ni agregados
indirectos. T09-003 sigue abierto.

## Varios records indirectos y escalares

La firma de referencia devuelve un record de 24 bytes y recibe `float`, dos
records del mismo tipo e `int`. Clang 23.1.0 describe un retorno `sret` y dos
argumentos `byval` en posiciones distintas (A:/temp/t09-indirect-mix-clang.ll).
El control C paso y NSC rechazaba la firma antes de la ampliacion
(A:/temp/nsc-t09-sysv-indirect-mix-_npqvkj1). La implementacion sigue las
posiciones y alineaciones del plan, crea una copia independiente por cada
record y aplica los atributos tanto a la declaracion como a la llamada; MIR y
el contrato vigente conservan esa forma.

Dos muestras, con alineacion de record de 1 y 8 bytes, pasaron ocho
comparaciones C/NS en estatica/DLL O0–O3, llamadas con argumentos nombrados y
plantillas precompiladas sin encabezado O0:
A:/temp/nsc-t09-sysv-indirect-mix-abrmvgg4 y
A:/temp/nsc-t09-sysv-aligned-mix-37_pi8k9. El productor modifica ambas
copias y las pruebas comprueban que los originales no cambien; la prueba LLVM
comprueba las posiciones de `sret` y ambos `byval`. La sonda de Clang rechaza
alineaciones inconsistentes y el control de metadatos comprueba la
reconstruccion (A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log).
Pasaron las regresiones del record indirecto simple de 24 bytes, de alineacion
16 y del control general con rechazo del caso aun pendiente
(A:/temp/nsc-t09-sysv-r24-m79c_gsn,
A:/temp/nsc-t09-sysv-align16-ind-124uzmzh y
A:/temp/nsc-t09-sysv-prueba-0e9g56gk).

Las muestras usan records de 24 bytes del mismo tipo que el retorno. Records
indirectos de tipos o tamanos diferentes, mezclas con otras clases de agregado
y otras firmas siguen pendientes; T09-003 permanece abierto.

La ampliacion posterior admite records indirectos de distintos tipos y
alineaciones cuando Clang confirma las posiciones y atributos. La muestra C
devuelve un record de 20 bytes y recibe records de 24 y 32 bytes, intercalados
con `float` e `int`; el control C paso y NSC rechazo inicialmente la firma
(A:/temp/nsc-t09-sysv-indirect-diff-hlqtimf2). La implementacion usa el tipo y
alineacion de cada argumento para crear su copia y sus atributos `byval`, y
conserva las alineaciones independientes en el contrato vigente.

La muestra paso ocho comparaciones estatica/DLL O0–O3, argumentos nombrados y
una plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-indirect-diff-o475ufe8). Una variante con retorno
alineado a 4, un argumento `byval` a 8 y otro a 16 paso las mismas pruebas
(A:/temp/nsc-t09-sysv-indirect-diff16-v91r7nwh). Las dos muestras modifican
ambas copias y comprueban que los originales sigan intactos. La sonda Clang
y los metadatos pasaron con alineaciones independientes
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Las
regresiones de argumentos indirectos del mismo tipo y el control general con
rechazo de un agregado directo mezclado pasaron en
A:/temp/nsc-t09-sysv-indirect-mix-1bgw8p7b,
A:/temp/nsc-t09-sysv-aligned-mix-5937l44e y
A:/temp/nsc-t09-sysv-prueba-hu37bgv6. Siguen pendientes mezclas de argumentos
indirectos con agregados directos u otras clases ABI; T09-003 continua abierto.

## Record directo junto a retorno y argumento indirectos

Una referencia C que devuelve un record de 20 bytes y recibe un record
directo de cuatro bytes, un record indirecto de 32 bytes, `float` e `int`
paso en estatica y DLL; NSC rechazaba la firma antes del cambio
(A:/temp/nsc-t09-sysv-direct-indirect-xf93zgz9). La declaracion LLVM ahora usa
los tipos fisicos del plan y el generador convierte el record directo a su
pieza entera, conservando la copia y atributos `byval` del otro argumento.

La muestra de cuatro bytes paso ocho comparaciones estatica/DLL O0–O3,
argumentos nombrados y plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-direct-indirect-m8e7mhmp). Una variante de tres bytes
paso las mismas comprobaciones con pieza `i24`
(A:/temp/nsc-t09-sysv-direct3-indirect-gwqof3xj). La sonda de Clang y el
control de metadatos verificaron los modos directo e indirecto
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron la
regresion de records indirectos de diferente alineacion y el control general
que conserva el rechazo de un record directo de dos piezas aun no admitido
(A:/temp/nsc-t09-sysv-indirect-diff16-xumzric7 y
A:/temp/nsc-t09-sysv-prueba-kkuz_ggj). Otras clases de record directo y firmas
quedan abiertas; T09-003 no se cierra.

Se ampliaron despues los argumentos directos a records clasificados por Clang
en dos piezas enteras. La referencia C con record directo de doce bytes y otro
indirecto paso mientras NSC rechazaba la firma
(A:/temp/nsc-t09-sysv-directpair-indirect-iexlbks8). El plan, MIR, generador y
metadatos conservan las posiciones de ambas piezas y del `byval` posterior.
La muestra paso ocho comparaciones estatica/DLL O0–O3, argumentos nombrados y
plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-directpair-indirect-nsujc6dz); la variante de dieciseis
bytes paso las mismas comprobaciones
(A:/temp/nsc-t09-sysv-directpair16-indirect-k1_jhb7l). La sonda Clang y el
control de metadatos pasaron (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log), al igual que las regresiones del record de
tres bytes, la alineacion indirecta 16 y el control general con rechazo de
una clase mixta de piezas aun no admitida
(A:/temp/nsc-t09-sysv-direct3-indirect-ih6jt_wh,
A:/temp/nsc-t09-sysv-indirect-diff16-eed8vwvp y
A:/temp/nsc-t09-sysv-prueba-5voqucsx). No se acreditan todas las clases de
agregado ni firmas posibles; T09-003 permanece abierto.

