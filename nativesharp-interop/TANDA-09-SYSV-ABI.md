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

## Records enteros de dos piezas con relleno final (T09-003)

Clang 23.1.0 clasifica los tres records probados, con un `long long` seguido
de `unsigned char`, `unsigned short` o `unsigned int`, como pares `i64`/`i8`,
`i64`/`i16` e `i64`/`i32`. Las muestras comprueban en C que cada record mide
16 bytes y tiene alineacion de 8; el IR de Clang verifica las piezas. Antes
del cambio el control C pasaba y NSC rechazaba los tres casos:
`A:/temp/nsc-t09-sysv-padded-integers-i8-mo1p718d`,
`A:/temp/nsc-t09-sysv-padded-integers-i16-fg77fot2` y
`A:/temp/nsc-t09-sysv-padded-integers-i32-84493b8k`.

Se admite la disposicion observada preservando el relleno y el plan de Clang.
Cada muestra pasa el record por valor, comprueba su copia y recibe el retorno,
junto con otros argumentos directos e indirectos. Pasaron ocho comparaciones
C/NativeSharp en estatica y DLL O0–O3, argumentos nombrados y plantilla
precompilada sin encabezado a O0:
`A:/temp/nsc-t09-sysv-padded-integers-i8-wez1joh_`,
`A:/temp/nsc-t09-sysv-padded-integers-i16-l0p42b_x` y
`A:/temp/nsc-t09-sysv-padded-integers-i32-gd3k22sa`.
Las pruebas de plan y metadatos pasaron (`A:/temp/t09-planes-clang.log` y
`A:/temp/t09-planes-identity.log`); las regresiones de pares enteros, pares con
relleno, records empaquetados y SysV general tambien pasaron.

El anterior control negativo usaba un record `i64`/`i32`, ahora admitido; se
conservo un control negativo para una funcion SysV variadica y se verifica su
diagnostico especifico (`A:/temp/nsc-t09-sysv-prueba-bwqts869`). La funcion
variadica y otras formas no probadas seguian pendientes en ese momento.
El avance posterior con variadicas escalares se detalla a continuacion.

## Variadicas SysV escalares: registros y pila (T09-003)

Clang 23.1.0 para Windows rechaza `va_start` dentro de una funcion SysV;
se reproduce en `A:/temp/t09-sysv-varargs-oracle/ir.log`. Por ello el productor
de la prueba es ensamblador compilado con Clang: comprueba valores de registros
enteros y XMM, el limite de registros vectoriales en AL y argumentos en pila.
El control C verifica las llamadas y el IR de Clang acredita las promociones.
Los ocho controles C pasaron con enlace estatico y DLL a O0–O3, mientras NSC
rechazaba la firma antes del cambio (`A:/temp/nsc-t09-sysv-varargs-lf5wfbiw`).

NSC ahora proyecta llamadas C SysV variadicas cuyo prefijo y retorno son
escalares admitidos; promueve los argumentos variables y emite la convencion
SysV. Se comprueban `sbyte`, `byte`, `short`, `ushort`, `int`, `uint`, `long`,
`float` promovido a `double`, `double` y `int*`, incluidos argumentos en pila.
El productor detecta valores o registros incorrectos. Pasaron ocho
comparaciones C/NativeSharp, la inspeccion de las llamadas LLVM, el rechazo
del record variadico y la plantilla precompilada con encabezado oculto
(`A:/temp/nsc-t09-sysv-varargs-tqxzi7ih`). La compilacion incremental y las
pruebas de plan y metadatos pasaron; tambien las regresiones C variadica cdecl
(`A:/temp/nsc-t09-llamadas-30zaqbgi`) y SysV general con control negativo y
callback rechazado (`A:/temp/nsc-t09-sysv-prueba-2egil0_g`).

El control negativo anterior se mantiene para una firma variadica con prefijo
de records no admitido; un control nuevo rechaza records en la elipsis. Esas
formas y otras combinaciones no probadas siguen pendientes. La muestra no
acredita la implementacion de `va_start` SysV de Clang ni compatibilidad
general. T09-003 permanece abierto.

### Punteros adicionales y limites de registros variadicos

El control C ejecutaba punteros a `short`, byte y record en la elipsis,
mientras NSC los rechazaba (`A:/temp/nsc-t09-sysv-varargs-6vef73o8`).
Tambien rechazaba un prefijo fijo con punteros a record y a `short`
(`A:/temp/nsc-t09-sysv-varargs-ueaivg9s`). Ahora se aceptan punteros de datos
en ambos lugares y como retorno; la muestra comprueba identidad, lecturas y
escrituras en los tipos citados, incluyendo un retorno de puntero a record.
Se conserva el rechazo de records pasados por valor en la elipsis.

La prueba tambien comprueba una elipsis vacia y nueve argumentos flotantes:
ocho ocupan registros y el ultimo la pila, con el contador AL que exige SysV.
Pasaron C y NativeSharp en estatica y DLL O0–O3, las comprobaciones de IR y
la plantilla precompilada sin encabezado (`A:/temp/nsc-t09-sysv-varargs-l994l0f0`).
Pasaron la compilacion incremental, los controles de plan y metadatos, la
regresion SysV general (`A:/temp/nsc-t09-sysv-prueba-br0h3slf`) y la regresion
variadica cdecl (`A:/temp/nsc-t09-llamadas-98t_fwt_`). Los records por valor,
callbacks SysV y combinaciones no probadas permanecen pendientes.

### Plan experimental de records en la elipsis

El IR de Clang muestra que el record `double`/`int` se descompone en dos
piezas cuando hay registros, pero se pasa `byval` si se agotan los registros
enteros o los flotantes. El oraculo reproducible
`pruebas-tanda-09-sysv-plan-variadico/validar.py` comprueba esos tres casos
(`A:/temp/nsc-t09-sysv-varrecord-ir-r42708hg`). Por tanto, clasificar el
record aisladamente no basta: hay que considerar los tipos de toda la llamada.

La infraestructura interna consulta `arrangeFreeFunctionCall` de Clang y copia
por valor el simbolo, destino, convencion, prefijo requerido, retorno y clases,
coerciones y posiciones logicas de los argumentos. Exige tipos efectivos ya
promovidos, comprueba el prefijo y rechaza atributos o formas no representados;
por ahora solo consulta C SysV para Windows x64. La sonda comprueba casos
directos e indirectos, agotamiento de ambos grupos de registros, rechazos y
supervivencia despues de destruir Clang y LLVM (`A:/temp/t09-planes-clang.log`).

Pasaron la compilacion incremental, los controles de plan y metadatos y la
regresion variadica (`A:/temp/nsc-t09-sysv-varargs-xvjzbpad`). Esta copia es
infraestructura interna, no una firma LLVM completa: falta obtener el plan
para cada llamada de NativeSharp, transportar y validar atributos y metadatos,
y emitir y ejecutar la llamada. Los records variadicos continuan rechazados y
T09-003 permanece abierto.

La sesion Clang ahora expone esa consulta experimental para tipos observados
en la propia sesion. Reconstruye tipos escalares, punteros y records nombrados,
exige identidad, tamaño y alineacion coherentes y rechaza tipos sin promocion,
identidades o tamaños incorrectos y sesiones cerradas. La sonda de sesion
comprueba records pequenos, mixtos y grandes, presion de registros enteros y
flotantes, punteros y uso del plan tras cerrar la sesion
(`A:/temp/t09-planes-clang.log`). Pasaron la compilacion incremental y las
regresiones de variadicas y records (`A:/temp/nsc-t09-sysv-varargs-mqb5op6c` y
`A:/temp/nsc-t09-sysv-padded-pair-id-008toa55`).

La identidad, tamaño y alineacion no prueban equivalencia estructural entre
sesiones: el futuro consumidor debe usar tipos de la misma sesion o verificar
esa equivalencia antes de solicitar el plan. La consulta todavia no esta
conectada a la emision ni a NSMeta, y no acredita records en la elipsis.

La copia experimental ahora incluye el tipo fisico de retorno, piezas fisicas
por argumento, atributos de extension y retorno oculto, y tipo y alineacion
de la memoria de los argumentos indirectos `byval`. Los parametros fijos se
contrastan con la declaracion LLVM y se rechazan discrepancias o formas no
representadas. La sonda verifica `i64`, `double`/`i32`, `byval`, presion de
registros, un prefijo fijo indirecto, extension `signext`, retorno `sret` y
supervivencia al cierre de Clang (`A:/temp/t09-planes-clang.log`).

Pasaron la compilacion incremental, la sonda y las regresiones dirigidas de
llamadas variadicas y records (`A:/temp/nsc-t09-sysv-varargs-hakk11_1` y
`A:/temp/nsc-t09-sysv-padded-pair-id-0pcbt3c_`). Aun debe integrarse el plan
por llamada con la conciliacion de origenes, HIR, MIR, emision y el contrato
vigente; las pruebas anteriores no demuestran ejecucion de records en la
elipsis. T09-003 sigue abierto.

### Conciliacion del origen de records variadicos

Se agrego una consulta que recibe la sesion de la funcion y el origen vivo de
cada argumento. Para records, incluso detras de punteros, comprueba identidad
y layout en la sesion de origen; si es distinta de la sesion de la funcion,
exige ademas la comparacion estructural de Clang antes de solicitar el plan.
La sonda prueba sesiones equivalentes, origen ausente o cerrado, tamaño
incorrecto y una pareja de records con el mismo nombre, USR, tamaño y
alineacion pero con campos `double` e `long long` incompatibles. El ultimo
caso se rechaza por la comprobacion estructural. Tambien verifica punteros y
que la copia del plan sobreviva al cierre (`A:/temp/t09-planes-clang.log`).

Pasaron la compilacion incremental y las regresiones de variadicas y records
(`A:/temp/nsc-t09-sysv-varargs-5ihcweqe` y
`A:/temp/nsc-t09-sysv-padded-pair-id-vxgbukmk`). El llamador debe asociar cada
tipo a su origen real y mantener vivas las sesiones durante la consulta; esa
asociacion todavia no esta conectada al analisis de llamadas. Las estructuras
por valor en la elipsis siguen rechazadas hasta integrar y probar toda la ruta.

### Validacion del plan variadico

La conciliacion comprueba ahora que el simbolo y el numero de parametros fijos
coincidan con la declaracion observada, y valida destino, convencion, retorno,
posiciones, coerciones, piezas, extensiones y argumentos indirectos. La sonda
acepta planes reales, incluidos `sret` y prefijos `byval`, y rechaza planes
alterados con simbolo, prefijo, posicion, pieza, tipo de retorno, alineacion o
atributos inconsistentes (`A:/temp/t09-planes-clang.log`). Las comprobaciones
del plan no sustituyen la conciliacion de procedencia ni prueban que el backend
pueda emitir la llamada.

Pasaron la compilacion incremental, las pruebas de plan y metadatos, el
oraculo de IR (`A:/temp/nsc-t09-sysv-varrecord-ir-yh_zwq5u`) y la regresion
variadica (`A:/temp/nsc-t09-sysv-varargs-5skgkebw`). En ese momento la
asociacion, el transporte y la emision seguian pendientes; el avance posterior
de transporte se describe a continuacion.

### Transporte del plan por llamada

Los nodos de HIR y MIR pueden conservar el plan especifico de una llamada;
la traduccion HIR–MIR lo mantiene y las identidades de cache de HIR y LLVM
incluyen su contenido. La cache interna de MIR lo codifica y restaura mediante
un formato acotado; datos truncados, longitudes invalidas y planes que exceden
sus limites se rechazan en vez de restaurar una llamada sin el plan. El codec
comprueba la estructura de los datos; el consumidor debe seguir validando el
contrato ABI y la procedencia antes de emitirlos.

Pasaron la construccion incremental agrupada y las pruebas dirigidas de una
llamada HIR sintetica, transporte a MIR, restauracion y cambio de identidad en
ambas caches (`A:/temp/t09-transport-directed/transport.log`). La sonda uso
planes reales de Clang, incluyo todas las truncaciones de un plan y probo la
restauracion de retornos indirectos (`A:/temp/t09-transport-directed/abi.log`).
Tambien pasaron la prueba existente de cache profunda
(`A:/temp/t09-cache-directed/cache.log`) y la regresion de llamadas variadicas
(`A:/temp/nsc-t09-sysv-varargs-u6d2_ifh`).

El plan aun no se asigna a llamadas reales ni se consume en LLVM. Tampoco se
ha agregado persistencia a NSMeta: las plantillas deben seguir rechazando los
records variables hasta completar esa ruta. El avance posterior de asociacion
escalar se describe a continuacion.

### Asociacion del plan a llamadas escalares reales

Cuando existe una sola referencia C de origen inequívoco, NSC localiza la
funcion por simbolo, USR y firma escalar, y consulta Clang con los tipos de
los argumentos opcionales ya promocionados en orden de ABI. El proveedor
rechaza identidades o firmas diferentes, referencias ambiguas y tipos fuera
del subconjunto observado. La asociacion actual cubre parametros fijos y
retorno escalares, y argumentos opcionales `int`, `uint`, `long`, `ulong` y
`double`; punteros y records aun no reciben plan mediante este proveedor.

Se verifico el plan en los volcados HIR y MIR de llamadas reales desde `Main`
y desde un metodo, con uno y cuatro trabajadores. Las muestras incluyen una
elipsis vacia, promociones y nueve valores flotantes. Pasaron ocho ejecuciones
C/NativeSharp con biblioteca estatica y DLL O0–O3, plantilla precompilada y
compilaciones fria y caliente con cache en A:
(`A:/temp/nsc-t09-sysv-varargs-vzcprhlu`). Las pruebas de sonda verifican
tipos fisicos, firma incorrecta y dos sesiones candidatas ambiguas
(`A:/temp/t09-transport-directed/abi.log`); tambien pasaron transporte y
cache de MIR (`A:/temp/t09-transport-directed/transport.log`).
Tambien pasaron las regresiones de llamadas C convencionales y SysV no
variadicas (`A:/temp/nsc-t09-llamadas-v5uajkqb` y
`A:/temp/nsc-t09-sysv-prueba-mdjcnlcb`).

La emision escalar seguia usando su ruta ya acreditada en ese momento. El
avance posterior con records se describe a continuacion.

### Records C en llamadas variadicas SysV directas

El proveedor resuelve records C completos con identidad y layout comprobados,
compara sus origenes vivos y consulta el plan de la llamada entera. La emision
valida ese plan, conserva el orden de evaluacion y transporta cada pieza directa
o una copia `byval`, con limites y alineacion verificados. La validacion ABI se
comparte con el backend; no depende de mantener vivo el contexto de Clang.

Antes del cambio pasaban los controles C y NSC rechazaba los records
(`A:/temp/nsc-t09-sysv-varargs-slhtza84`). Pasaron despues seis formas: record
entero pequeno, record mixto `double`/`int`, dos records en la misma llamada,
record mixto indirecto por agotamiento de registros enteros o flotantes y
record grande indirecto. El productor ensamblado verifica registros, pila y
contador AL; modifica las copias indirectas y los consumidores comprueban que
sus originales no cambien. Los IR C y NS coinciden en las piezas y `byval`.
Pasaron estatica y DLL O0–O3, HIR/MIR con uno y cuatro trabajadores, cache
fria/caliente y el rechazo de un struct NativeSharp no admitido
(`A:/temp/nsc-t09-sysv-varargs-ml6qaxe_`).

Tambien pasaron la sonda de ABI, la prueba de transporte y las regresiones de
llamadas C, SysV y pares con relleno (`A:/temp/t09-transport-directed/abi.log`,
`A:/temp/nsc-t09-llamadas-a9bodxcu`, `A:/temp/nsc-t09-sysv-prueba-czh6juvn`,
`A:/temp/nsc-t09-sysv-padded-pair-id-0ariwi50`). La plantilla escalar
precompilada sigue pasando, pero una plantilla con record variable y encabezado
oculto se rechaza (`pre-record-pendiente.log` en la evidencia anterior). Falta
transportar esa dependencia bajo el unico contrato vigente de NSMeta, y tambien
quedan abiertas otras firmas, tipos y formas de origen. T09-003 sigue abierto.

#### Sonda de reconstruccion desde el catalogo

Como preparacion se agrego un reconstruidor experimental de records C planos
con campos escalares, todavia desconectado del compilador. La prueba obtiene un
record mixto de Clang, lo captura, serializa y recupera mediante el catalogo C,
reconstruye una declaracion en memoria y comprueba cada tipo, desplazamiento,
tamano y alineacion. Clang produce el mismo plan para la llamada sin presion y
con registros enteros o flotantes agotados. Tambien se rechazan campos de tipo
incompatible aunque tengan igual tamano, offsets o alineaciones alterados,
bitfields, texto de tipo inesperado y otro destino. Pasaron compilacion
incremental y sondas ABI, transporte e identidad
(`A:/temp/t09-build-replay.log` y `A:/temp/t09-transport-directed/`).

Esto no prueba que el catalogo actual conserve todos los atributos relevantes
para la ABI ni vincula la declaracion recreada con la funcion y el origen
originales. Antes de usarlo en el consumidor hay que acreditar y transportar
esas garantias, resolver la seleccion entre catalogos y cubrir tipos no planos.
La plantilla precompilada con record permanece pendiente; no se ha ampliado el
contrato de interoperabilidad ni se ha cerrado T09-003.

Se agrego una consulta a la sesion viva de Clang para reconocer una sintaxis C
plana: una definicion unica, completa, sin atributos en record o campos y con
campos builtin sin calificadores. La sonda acepta un record ordinario y rechaza
atributos `packed` y `aligned`, `#pragma pack`, alias, `const`, bitfields,
punteros y arreglos; una declaracion incompleta, una sesion C++ o cerrada no
proporcionan observacion. La compilacion incremental y las pruebas ABI,
transporte e identidad pasaron (`A:/temp/t09-build-replay.log` y
`A:/temp/t09-transport-directed/`). En esa etapa la observacion aun no se
publicaba; por si sola no demuestra equivalencia ABI o procedencia y tampoco
habilita llamadas precompiladas.

La observacion se transporta ahora en el catalogo C del contrato vigente. El
productor solo la activa para una identidad y layout elegibles cuando todas
las definiciones completas asociadas a sus sesiones de Clang la confirman; el
codec rechaza una observacion incompatible con los datos del record. La
observacion no modifica la identidad C: catalogos repetidos pueden deduplicar
la declaracion aun si uno carece de ella, y cualquier consumidor futuro debe
reconciliar la observacion por separado. Una prueba de productor real publico
un record plano y conservo el atributo desactivado para otro `packed`; tambien
pasaron el round trip, las pruebas de corrupcion y la deduplicacion
(`A:/temp/t09-transport-directed/`).

Pasaron las regresiones dirigidas C y SysV, con biblioteca estatica y DLL en
O0–O3 (`A:/temp/nsc-t09-llamadas-8cgzodmp`,
`A:/temp/nsc-t09-sysv-prueba-11l0kzt_`,
`A:/temp/nsc-t09-sysv-padded-pair-id-jppawpqi` y
`A:/temp/nsc-t09-sysv-varargs-5_z7sj51`). La ultima confirma que la plantilla
variadica con record precompilado aun se rechaza: falta vincular la funcion,
el tipo y la observacion al consultar el plan, y utilizarlo en el consumidor.

El catalogo tambien conserva, cuando Clang puede producirlo desde la referencia
exacta seleccionada, el plan variadico de la funcion para el prefijo fijo sin
argumentos opcionales. El codec comprueba simbolo, convencion, aridad y forma
del plan; no inventa uno si la sesion o la observacion son ambiguas. Una prueba
con un productor real verifica publicacion y lectura, y rechaza un plan ligado
a otro simbolo (`A:/temp/t09-transport-directed/`). Pasaron tambien las
regresiones de llamadas C, SysV y variadicas
(`A:/temp/nsc-t09-llamadas-1ka10mpk`, `A:/temp/nsc-t09-sysv-prueba-frw0dj7w`,
`A:/temp/nsc-t09-sysv-varargs-jv7actay`). Este plan base no clasifica los
records opcionales ni habilita el consumidor: falta reconstruir y comparar la
declaracion con su origen, reconciliar observaciones y consultar la llamada
completa. T09-003 permanece abierto.

#### Records planos desde una plantilla precompilada

El consumidor selecciona la funcion y cada record por modulo, simbolo, USR,
identidad y layout; exige la observacion plana y el plan fijo del productor.
Reconstruye las declaraciones admitidas en una sesion de Clang en memoria,
verifica cada campo y compara el plan del prefijo antes de consultar la llamada
completa. El acceso a la fabrica de sesiones se serializa entre trabajadores.
La emision utiliza el mismo transporte de piezas o copias `byval` ya validado.

El fixture con encabezado oculto ejecuta records pequeno, mixto y grande,
dos records en la misma llamada y agotamiento de registros enteros y flotantes.
Pasaron estatica y DLL O0–O3, HIR y MIR con uno y cuatro trabajadores, piezas y
alineacion en LLVM, cache fria/caliente y rechazo de record empaquetado
(`A:/temp/nsc-t09-sysv-varargs-2oqpbfv0`). Pasaron las sondas de transporte,
ABI e identidad y las regresiones de llamadas C y SysV
(`A:/temp/t09-transport-directed/`, `A:/temp/nsc-t09-llamadas-yklssfjq`,
`A:/temp/nsc-t09-sysv-prueba-dk0t990b` y
`A:/temp/nsc-t09-sysv-padded-pair-id-qjkigrcg`).

En esa etapa la reconstruccion admitia records C planos de campos escalares sin
atributos y las formas escalares y de puntero que se describen despues; rechazaba
catalogos ambiguos, records empaquetados o anidados y otras formas que no puede
comprobar. Exige Clang configurado en el consumidor. No
acredita callbacks SysV, compatibilidad C++ general ni cierre de T09-003.

La llamada completa admite tambien un puntero fijo `int*` y uno variadico
`short*` junto a un record mixto por valor, con retorno `int` o `int*`. El
productor comprueba los registros SysV, el contador AL y los valores, modifica
los dos destinos y devuelve el mismo puntero en el segundo caso. El control C
paso mientras el NSC anterior rechazaba la combinacion
(`A:/temp/nsc-t09-sysv-varargs-egxt94zf`); despues pasaron C y NativeSharp,
directos y mediante plantilla precompilada sin encabezado, en estatica y DLL
O0–O3, con HIR/MIR de uno y cuatro trabajadores, comparacion LLVM y cache
fria/caliente (`A:/temp/nsc-t09-sysv-varargs-2pi2f5zs`). Tambien pasaron las
sondas de transporte, ABI e identidad y las regresiones C/SysV
(`A:/temp/t09-transport-directed/`, `A:/temp/nsc-t09-llamadas-l518teid`,
`A:/temp/nsc-t09-sysv-prueba-mlzsk3ef`,
`A:/temp/nsc-t09-sysv-padded-pair-id-fpdt6oxk`). En ese momento los punteros a
records en esta ruta seguian pendientes de evidencia.

Se comprobo despues una llamada con record mixto por valor y puntero variadico
a otro record C plano. El productor verifica los registros, AL y los campos,
y modifica el record apuntado; el llamador comprueba la escritura y conserva
el record pasado por valor. El control C paso antes de la correccion, mientras
NSC rechazaba la llamada (`A:/temp/nsc-t09-sysv-varargs-fu7k40i0`). La ruta
directa contrasta la identidad y el origen de cada record; la precompilada
recupera y verifica el record apuntado antes de consultar a Clang. Pasaron
estatica y DLL O0–O3, plantilla sin encabezado, HIR/MIR con uno y cuatro
trabajadores, LLVM, cache y rechazos de record empaquetado por valor y por
puntero (`A:/temp/nsc-t09-sysv-varargs-0w1qmdlb`). Pasaron las sondas ABI,
transporte e identidad y las regresiones C/SysV
(`A:/temp/t09-transport-directed/`, `A:/temp/nsc-t09-llamadas-qlbak5x9`,
`A:/temp/nsc-t09-sysv-prueba-dcr6w22o`,
`A:/temp/nsc-t09-sysv-padded-pair-id-bk6dhgji`). En ese momento el puntero fijo
o retorno de puntero a record en esta combinacion seguia pendiente.

Se comprobo despues un puntero fijo a record C y retorno del mismo puntero,
junto a un record mixto por valor y un `short*` variadico. El productor verifica
los registros SysV, AL y los valores, modifica ambos destinos y retorna el
puntero original. Clang paso antes del cambio, mientras NSC rechazaba la
llamada (`A:/temp/nsc-t09-sysv-varargs-e_x9jijx`). En la ruta precompilada se
verifica la identidad original de cada tipo y se compara el plan del prefijo
con el observado por Clang; el puntero se representa como puntero opaco en la
declaracion temporal. Pasaron llamadas directas y plantilla sin encabezado,
estatica y DLL O0–O3, HIR/MIR, LLVM y cache
(`A:/temp/nsc-t09-sysv-varargs-tsglggvf`). Pasaron las sondas ABI, identidad y
transporte y las regresiones C/SysV (`A:/temp/t09-transport-directed/`,
`A:/temp/nsc-t09-llamadas-hbq7mj6p`, `A:/temp/nsc-t09-sysv-prueba-drplfe1l`,
`A:/temp/nsc-t09-sysv-padded-pair-id-31xfd4z0`). En ese momento los records
empaquetados precompilados seguian pendientes.

Se amplio la reconstruccion a records C con el atributo `packed` observado por
Clang en todas sus definiciones y campos escalares sin otros atributos. Se
conserva esa observacion en el contrato vigente y se verifica el layout
reconstruido antes de solicitar el plan; observaciones contradictorias o
atributos combinados aun no representados se rechazan. La muestra usa un record
de nueve bytes con `double` desalineado: Clang lo pasa con `byval`, el productor
altera la copia y el llamador comprueba que el original no cambio. Tambien se
comprueba lectura y escritura a traves de un puntero a ese record.

El control C y la llamada directa pasaban, pero la plantilla precompilada se
rechazaba antes del cambio (`A:/temp/nsc-t09-sysv-varargs-nkhpdtcv`). Pasaron
despues estatica y DLL O0–O3, plantilla sin encabezado, HIR/MIR, LLVM, cache y
controles negativos para atributos combinados
(`A:/temp/nsc-t09-sysv-varargs-91crtgf3`). Pasaron las sondas de ABI, identidad,
transporte y las regresiones C/SysV (`A:/temp/t09-transport-directed/`,
`A:/temp/nsc-t09-llamadas-64i3ab_r`, `A:/temp/nsc-t09-sysv-prueba-5u2x7j0i`,
`A:/temp/nsc-t09-sysv-padded-pair-id-t4nwi_td`). En ese momento los records
anidados, atributos combinados y otras formas seguian pendientes.

La reconstruccion admite tambien campos de arreglos C de tamano conocido con
elementos escalares, comprobando dimensiones, tamano, alineacion y el tipo de
cada elemento. La muestra con dos `double` compara las dos piezas en registros
y el contador AL de Clang y NativeSharp; el control directo pasaba y la
plantilla precompilada se rechazaba antes del cambio
(`A:/temp/nsc-t09-sysv-varargs-pzrssr6d`). Pasaron despues las ocho variantes
estatica/DLL O0–O3, plantilla sin encabezado, HIR/MIR, LLVM, cache y rechazo
de dimensiones inconsistentes (`A:/temp/nsc-t09-sysv-varargs-e9zkdcrx`,
`A:/temp/t09-transport-directed/`). Tambien pasaron las regresiones C/SysV
(`A:/temp/nsc-t09-llamadas-0uyfvhht`, `A:/temp/nsc-t09-sysv-prueba-d00229lp`,
`A:/temp/nsc-t09-sysv-padded-pair-id-c5m49yb5`). Esa evidencia cubria el
arreglo probado; en ese momento los records anidados seguian abiertos.

Se agrego la reconstruccion de dependencias de records C nombrados: el
consumidor localiza cada tipo interior por identidad y USR, comprueba su layout
y observacion, ordena las declaraciones y verifica cada campo reconstruido.
La muestra contiene un `double` y un record interior de dos `int`; el productor
comprueba las piezas y el llamador verifica que el valor original se conserva.
Antes del cambio pasaban C y la llamada directa, pero la plantilla sin encabezado
se rechazaba (`A:/temp/nsc-t09-sysv-varargs-bmlj7x6h`).

Pasaron estatica y DLL O0–O3, plantilla precompilada, HIR/MIR, comparacion LLVM,
cache, rechazo de identidad interior incorrecta y rechazo de una dependencia con
atributos aun no representados (`A:/temp/nsc-t09-sysv-varargs-xp1p031e`,
`A:/temp/t09-transport-directed/`). Pasaron las regresiones C/SysV
(`A:/temp/nsc-t09-llamadas-ap4h1n_r`, `A:/temp/nsc-t09-sysv-prueba-dpo4s1rk`,
`A:/temp/nsc-t09-sysv-padded-pair-id-4gum6931`). La evidencia cubre la muestra
anidada indicada; en ese momento aliases, estructuras anonimas y otras
combinaciones aun no observadas permanecian pendientes. T09-003 no se cierra.

Se ampliaron las comprobaciones de la reconstruccion ya implementada con un
arreglo de dos records interiores, un arreglo de tres records que Clang pasa
por `byval`, y una union nombrada dentro de un record. Los productores verifican
las piezas o la copia indirecta y el llamador comprueba que su valor original
se conserva; la copia indirecta se modifica para detectar aliasing. Pasaron C
y NativeSharp con estatica y DLL O0–O3, tambien desde plantilla precompilada
sin encabezado, con inspeccion HIR/MIR y LLVM y cache fria/caliente
(`A:/temp/nsc-t09-sysv-varargs-ce0tl155`). Esta es evidencia de las tres formas
indicadas; otras estructuras y atributos siguen pendientes.

La observacion de Clang sigue ahora cadenas `typedef` sin atributos hasta su
tipo subyacente, manteniendo las comprobaciones de cualificadores, identidad y
layout. La muestra combina un alias escalar encadenado y un alias de record en
un mismo argumento variadico. Antes del cambio pasaban C y la llamada directa,
pero fallaba la plantilla precompilada sin encabezado
(`A:/temp/nsc-t09-sysv-varargs-aw0r201h`). Despues pasaron estatica y DLL
O0–O3, HIR/MIR, comparacion de piezas LLVM, cache y rechazo de un alias con
alineacion explicita (`A:/temp/nsc-t09-sysv-varargs-5s8lne7r`). Tambien
pasaron las sondas ABI, identidad y transporte y las regresiones C/SysV
(`A:/temp/t09-transport-directed/`, `A:/temp/nsc-t09-llamadas-wfbej62c`,
`A:/temp/nsc-t09-sysv-prueba-gt1dt3bv`,
`A:/temp/nsc-t09-sysv-padded-pair-id-qzcg_kr_`). Aliases con atributos y otras
formas permanecen pendientes.

La reconstruccion admite campos de bits C nombrados con ancho positivo y tipo
entero observado por Clang; comprueba el ancho y el desplazamiento de cada
campo antes de usar el plan. La muestra de cuatro campos `unsigned int`
comprueba el patron completo que recibe el productor y conserva el valor del
llamador. Antes del cambio pasaban C y la llamada directa, pero la plantilla
precompilada se rechazaba (`A:/temp/nsc-t09-sysv-varargs-f8_ku9cf`). Despues
pasaron estatica y DLL O0–O3, plantilla sin encabezado, HIR/MIR, comparacion
LLVM y cache (`A:/temp/nsc-t09-sysv-varargs-3ax9raie`). Las sondas rechazan
anchos o desplazamientos distintos y, en ese momento, campos anonimos
(`A:/temp/t09-transport-directed/`). Tambien pasaron las regresiones C/SysV
(`A:/temp/nsc-t09-llamadas-esthiyol`, `A:/temp/nsc-t09-sysv-prueba-vbi5bsq0`,
`A:/temp/nsc-t09-sysv-padded-pair-id-8xhqfjoa`). En ese momento, campos de bits
anonimos o de ancho cero y otras formas no acreditadas seguian pendientes.

La reconstruccion preserva ahora la ausencia de nombre y el ancho cero en
campos de bits enteros, y contrasta anchos y desplazamientos contra Clang. La
muestra incluye un campo anonimo de cinco bits y otro de ancho cero entre
campos `unsigned int`; el productor comprueba unicamente los bits nombrados,
sin exigir valores para el relleno. Antes del cambio, la llamada directa paso
en estatica y DLL O0–O3 pero la plantilla precompilada fallo
(`A:/temp/nsc-t09-sysv-varargs-pm6k2sh2`). Despues pasaron esas ocho
comparaciones, la plantilla precompilada sin encabezado, los planes HIR/MIR y
las piezas LLVM (`A:/temp/nsc-t09-sysv-varargs-x8rqtbh_`). Las pruebas
dirigidas de transporte, ABI e identidad pasaron (`A:/temp/t09-transport-directed/`),
incluido el contraste de offsets y anchos alterados. Pasaron las regresiones
de llamadas, SysV y relleno (`A:/temp/nsc-t09-llamadas-451wwwvl`,
`A:/temp/nsc-t09-sysv-prueba-bphihlmg`,
`A:/temp/nsc-t09-sysv-padded-pair-id-qpavo23m`). Esto acredita solo la muestra
indicada; otros tipos de campos, layouts y convenciones siguen pendientes.

Para `_Bool` se contrasto un record con dos campos de bits de un bit y un
campo ordinario, comprobando ambos valores verdaderos y el falso sin depender
del relleno. Antes del cambio, C y NativeSharp pasaban en estatica y DLL O0–O3,
pero se rechazaba la plantilla precompilada
(`A:/temp/nsc-t09-sysv-varargs-u6d4k_3p`). La reconstruccion conserva `_Bool`
y rechaza anchos mayores a uno; la sonda comprueba tambien un desplazamiento
alterado. Despues pasaron la compilacion incremental, transporte, ABI e
identidad (`A:/temp/t09-transport-directed/`) y las ocho comparaciones con
la plantilla precompilada sin encabezado y planes HIR/MIR/LLVM
(`A:/temp/nsc-t09-sysv-varargs-bag0zyqp`). Pasaron las regresiones previas al
refuerzo de ancho (`A:/temp/nsc-t09-llamadas-rw4h8bv_`,
`A:/temp/nsc-t09-sysv-prueba-vhripc34`,
`A:/temp/nsc-t09-sysv-padded-pair-id-_w27v5qo`). Esta evidencia cubre la
muestra descrita, no todos los layouts booleanos ni las demas convenciones.

## Puntero de funcion SysV devuelto por una biblioteca

El control `pruebas-tanda-09-sysv-indirecta` obtiene un puntero de
una biblioteca C y llama seis argumentos enteros en orden, con suma ponderada.
Clang 23.1.0 emitio `x86_64_sysvcc` para la llamada indirecta y su consumidor
termino con codigo cero en estatica O0. NSC rechazo la importacion con
`unsupported C callback calling convention`, antes de emitir la llamada
(`A:/temp/nsc-t09-sysv-indirecta-k_gky9w0`). Ese diagnostico no acreditaba soporte.

La inspeccion inicial localiza el rechazo en `QualifiedTypeNames.cpp` y
`CppSemanticFunction.cpp`. La convencion de Clang ya se observa en el tipo,
pero las identidades estructurales y la emision indirecta necesitan transportar
una convencion SysV distinguible. La implementacion incorpora esa convencion
a la identidad del tipo y emite `x86_64_sysvcc` en llamadas indirectas de la
firma comprobada de enteros. En ese primer cambio rechazaba otras firmas y la
conversion de metodos o lambdas hacia estos punteros; conservaba el rechazo de
convertir una funcion SysV a un callback de otra convencion.

La muestra inicial paso ejecucion C y NativeSharp en estatica y DLL O0–O3 y una
plantilla precompilada sin encabezado O0; el LLVM de las llamadas directa y
precompilada contiene la convencion SysV
(`A:/temp/nsc-t09-sysv-indirecta-matriz-bhcgvs6s`). La prueba puntual tambien
rechazo una firma flotante no admitida y una conversion de funcion C ordinaria
a puntero SysV (`A:/temp/nsc-t09-sysv-indirecta-_9p96ymx`). Pasaron transporte,
ABI, identidad y el control SysV previo, incluido el rechazo de conversion
opuesta (`A:/temp/t09-transport-directed/`, `A:/temp/nsc-t09-sysv-prueba-in_if80d`).
Las pruebas CTest `item344`, `item345`, `item333.win64-vararg-abi` y
`project-loader` pasaron tras crear sus directorios temporales aislados: el
primer intento fallo porque esos directorios aun no existian. Este resultado
no acreditaba argumentos o retornos distintos de `int`, variadicas, agregados,
ni la creacion de callbacks NativeSharp con convencion SysV.

Posteriormente se ampliaron llamadas indirectas escalares no variadicas: la
muestra incorpora una firma mixta `double`/`float`/`int`, retorno `float`,
`long` con valor fuera de 32 bits, `uint` y `ulong` con valores grandes,
puntero con escritura y retorno de identidad, y retorno `void`. Antes de la
ampliacion el control C pasaba y NSC rechazaba la firma
(`A:/temp/nsc-t09-sysv-indirecta-bg4a7j7i`); el primer intento de control C
necesito agregar `_fltused` para el enlace sin runtime
(`A:/temp/nsc-t09-sysv-indirecta-br66nrki`). Despues pasaron ocho comparaciones
C/NS en estatica y DLL O0–O3, con plantilla precompilada sin encabezado O0 y
verificacion LLVM de los tipos de retorno (`A:/temp/nsc-t09-sysv-indirecta-matriz-10gqw56l`).
Se conserva el rechazo de firma no admitida con un argumento record, ademas
del rechazo de conversion incompatible (`A:/temp/nsc-t09-sysv-indirecta-7roifs2d`).
Pasaron transporte, ABI e identidad (`A:/temp/t09-transport-directed/`) y las
regresiones de llamadas, SysV y variadicas (`A:/temp/nsc-t09-llamadas-b5nmhjsw`,
`A:/temp/nsc-t09-sysv-prueba-sniutyu0`, `A:/temp/nsc-t09-sysv-varargs-rud7djst`).
Booleanos, enteros estrechos, variadicas, agregados y creacion de callbacks
siguen pendientes en la ruta indirecta; no se acredita cada combinacion posible.

## Retorno indirecto sin argumentos byval

Dos controles C pasaron mientras NSC rechazaba las firmas: una funcion sin
parametros y otra que recibe `float`, un record directo `double`/`i32` e
`int`, ambas con retorno indirecto de 20 bytes y sin argumentos `byval`
(A:/temp/nsc-t09-sysv-sret-empty-z9woiblv y
A:/temp/nsc-t09-sysv-sret-mixed-7gjer3n_). Se amplio el consumo del plan para
esas formas sin exigir un argumento indirecto; el retorno y sus posiciones
siguen determinados por Clang.

Ambas muestras pasaron las ocho comparaciones estatica/DLL O0–O3 y una
plantilla precompilada sin encabezado O0; la muestra con parametros incluye
argumentos nombrados (A:/temp/nsc-t09-sysv-sret-empty-cro5g_bd y
A:/temp/nsc-t09-sysv-sret-mixed-h3wj0z0u). La sonda de Clang y el control de
metadatos verificaron las dos formas (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Pasaron la regresion con pieza vectorial y
argumento indirecto y el control general con su rechazo pendiente
(A:/temp/nsc-t09-sysv-class-vf-gf8vy5gm y
A:/temp/nsc-t09-sysv-prueba-07ho5f3y). No se acreditan todas las firmas ni
las clases de agregado; T09-003 sigue abierto.

## Clases adicionales de piezas directas con retorno indirecto

Se amplio la validacion de las clases fisicas observadas por Clang para
records directos de dos piezas que acompanan un retorno y un argumento
indirectos. La conservacion del plan registra las clases y anchuras fisicas,
incluido el elemento de los vectores fijos de dos `float`; la decodificacion
rechaza descriptores invalidos. Los controles C para las primeras muestras
pasaron y NSC las rechazaba antes del cambio: entero/`double`
(A:/temp/nsc-t09-sysv-class-id-s56t2dnz), `double`/`double`
(A:/temp/nsc-t09-sysv-class-dd-oikzn_6k) y vector/`float`
(A:/temp/nsc-t09-sysv-class-vf-6xbrvxf1).

Cada muestra siguiente paso ocho comparaciones C/NS estatica/DLL O0–O3,
argumentos nombrados y una plantilla precompilada sin encabezado O0:

- Entero/`double`: A:/temp/nsc-t09-sysv-class-id-1eo_aw9w.
- `double`/`double`: A:/temp/nsc-t09-sysv-class-dd-v5bkhtd0.
- Vector/`float`: A:/temp/nsc-t09-sysv-class-vf-bl02ysby.
- `double`/`float` y `float`/`double`: A:/temp/nsc-t09-sysv-class-df-k4el20im y A:/temp/nsc-t09-sysv-class-fd-_jrcwm0l.
- Entero/`float` y entero/vector: A:/temp/nsc-t09-sysv-class-if-laupd60g y A:/temp/nsc-t09-sysv-class-iv-pgcs5khm.
- Vector/entero de 64 y 32 bits: A:/temp/nsc-t09-sysv-class-vi-nirbosd1 y A:/temp/nsc-t09-sysv-class-vs-5i8388el.
- Vector/vector, incluido alineado a 16: A:/temp/nsc-t09-sysv-class-vv-ovyez3n6 y A:/temp/nsc-t09-sysv-class-vv16-snx7u73a.
- `double`/vector y vector/`double`: A:/temp/nsc-t09-sysv-class-dv-5_qgdqyb y A:/temp/nsc-t09-sysv-class-vd-mh574f86.

Las muestras verifican los datos de las piezas y que las copias no modifiquen
los originales. La sonda de Clang y el control de metadatos, incluido el
rechazo de un descriptor corrupto, pasaron en A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log. Pasaron la regresion `double`/entero y el
control general con su rechazo aun pendiente
(A:/temp/nsc-t09-sysv-mixedpair-indirect-go_d9jdw y
A:/temp/nsc-t09-sysv-prueba-2ezkq4v8). Estos ejemplos no certifican todos los
layouts, vectores u otras firmas; T09-003 permanece abierto.

Tras reforzar la validacion de atributos directos y del elemento vectorial,
se repitieron las trece muestras anteriores: todas pasaron las ocho
combinaciones y la precompilacion. Los registros agrupados por caso estan en
`A:/temp/t09-clase-agrupada-<caso>.log`; la sonda y los controles de
metadatos tambien pasaron y rechazan atributos y elementos inconsistentes.

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

## Piezas flotantes con alineacion de ocho bytes

Clang 23.1.0 clasifica las muestras con alineacion de ocho bytes en piezas
`float`/`i64`, `i64`/`float`, `float`/vector y vector/`float`; las firmas
exactas estan en A:/temp/t09-float-alineado-*-clang.ll. Antes del cambio los
controles C pasaron y NSC rechazo esas cuatro firmas, respectivamente en
A:/temp/nsc-t09-sysv-float-aligned-fi64-yk_psn_w,
A:/temp/nsc-t09-sysv-float-aligned-i8f-1jqtmrwh,
A:/temp/nsc-t09-sysv-float-aligned-fv-k2b2g7ua y
A:/temp/nsc-t09-sysv-float-aligned-vf-p62r8y7i.

El plan admite las piezas y el tamano de 16 bytes alineado a ocho; las cuatro
muestras verifican argumento y retorno, campos, copias y `byval`, y pasaron
ocho comparaciones C/NS en estatica y DLL O0–O3 y plantilla precompilada sin
encabezado O0, respectivamente en
A:/temp/nsc-t09-sysv-float-aligned-fi64-v6809lpc,
A:/temp/nsc-t09-sysv-float-aligned-i8f-eazntrwv,
A:/temp/nsc-t09-sysv-float-aligned-fv-3vy7_3p0 y
A:/temp/nsc-t09-sysv-float-aligned-vf-nxb4siui.

Una quinta muestra alinea un byte despues de un `float`; Clang transporta el
primer campo y su relleno como `double`. Ya pasaba antes del cambio
(A:/temp/nsc-t09-sysv-float-aligned-fi8-tgpvuu3b) y paso nuevamente en
A:/temp/nsc-t09-sysv-float-aligned-fi8-oz_4yg0_; se conserva como regresion,
no como capacidad nueva. La sonda y los metadatos pasaron
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron las
regresiones del layout `i64`/`float` previo, vector/`float`, par con relleno y
record empaquetado en A:/temp/nsc-t09-sysv-mixed-if-aj7qk5yv,
A:/temp/nsc-t09-sysv-class-vf-8l45yvf1,
A:/temp/nsc-t09-sysv-padded-pair-dc-e5osspbp y
A:/temp/nsc-t09-sysv-packed-small-mixed-irektwlr. El control general mantiene
el rechazo de `i64`/`i32` con relleno final y de callback
(A:/temp/nsc-t09-sysv-prueba-g9x35wij). Otras formas siguen abiertas;
T09-003 y la tanda 9 no se cierran.

## Record empaquetado pequeno pasado indirectamente

Clang 23.1.0 pasa un record empaquetado de nueve bytes con `unsigned char` y
`double` desalineado mediante `byval` alineado a ocho; cuando lo devuelve usa
`sret` alineado a uno (A:/temp/t09-packed-small-oracle.ll). El tamano por si
solo no indica si un record se pasa directamente. Los controles C de tres
firmas pasaron y NSC las rechazaba antes del cambio:
A:/temp/nsc-t09-sysv-packed-small-identity-sectpn53,
A:/temp/nsc-t09-sysv-packed-small-scalar-m2ifzsbp y
A:/temp/nsc-t09-sysv-packed-small-mixed-n_vcv9p4.

NSC usa el modo y alineaciones del plan observado por Clang y el contrato
vigente conserva explicitamente cuando un argumento pequeno es indirecto.
Las tres muestras pasaron ocho comparaciones C/NS, estatica y DLL O0–O3,
comprobacion de atributos LLVM, copias y plantilla precompilada sin encabezado
O0: A:/temp/nsc-t09-sysv-packed-small-identity-x7xnnpjk,
A:/temp/nsc-t09-sysv-packed-small-scalar-xs2ezii4 y
A:/temp/nsc-t09-sysv-packed-small-mixed-gxfqtue4. Dos muestras agregadas
despues, con el argumento empaquetado y retornos directos de una y dos piezas,
pasaron las mismas pruebas en
A:/temp/nsc-t09-sysv-packed-small-return-single-w801xfpi y
A:/temp/nsc-t09-sysv-packed-small-return-pair-5o_o6crj; no se afirma un
rechazo previo ejecutado para estas variantes.

La sonda y los metadatos pasaron, incluido el rechazo de un descriptor
contradictorio (A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log).
Las regresiones del record indirecto de 24 bytes, retorno directo mixto y
record con relleno pasaron en A:/temp/nsc-t09-sysv-r24-viiv_4mo,
A:/temp/nsc-t09-sysv-return-record-mixed-ktlzlvo4 y
A:/temp/nsc-t09-sysv-padded-pair-ds-9ah52plb. El primer intento de conservar
un control negativo con firma variadica produjo un rechazo anterior a HIR con
un diagnostico distinto (A:/temp/nsc-t09-sysv-prueba-cg_y2r4t); el control se
replanteo con un record `float`/`i64` aun no admitido por esta ruta y paso en
A:/temp/nsc-t09-sysv-prueba-gthf82qy. La llamada variadica sigue pendiente;
su rechazo no acredita interoperabilidad. Otras disposiciones empaquetadas
tambien quedan abiertas; T09-003 y la tanda 9 no se cierran.

## Records de dos piezas con relleno

Clang 23.1.0 clasifica records con relleno como `i8`/`double`, `i16`/`double`,
`i32`/`double`, `double`/`i8`, `double`/`i16`, vector/`i8` y vector/`i16`.
La segunda pieza comienza a ocho bytes aunque el primer entero sea estrecho;
en los dos casos vectoriales el tamano del record incluye relleno final. Las
firmas exactas de las siete muestras estan en
A:/temp/t09-par-relleno-*-clang.ll; la investigacion inicial incluye ademas
A:/temp/t09-oraculo-relleno.ll.

Cada muestra recibe y devuelve el record con relleno, junto a otro record
directo, otro `byval` y escalares. Antes del cambio los controles C pasaron y
NSC rechazo las siete firmas, respectivamente en
A:/temp/nsc-t09-sysv-padded-pair-cd-cna9tral,
A:/temp/nsc-t09-sysv-padded-pair-sd-pwsv9rp6,
A:/temp/nsc-t09-sysv-padded-pair-id-occarjc7,
A:/temp/nsc-t09-sysv-padded-pair-dc-23rnkrjj,
A:/temp/nsc-t09-sysv-padded-pair-ds-ackkgsbp,
A:/temp/nsc-t09-sysv-padded-pair-vc-ja70lca0 y
A:/temp/nsc-t09-sysv-padded-pair-vs-ko62eh5w.

El plan admite las piezas observadas y valida el tamano y alineacion del
record, incluido el relleno; el generador mantiene el desplazamiento de ocho
bytes y el contrato vigente conserva las clases. Las siete muestras pasaron
ocho comparaciones C/NS en estatica y DLL O0–O3, argumentos nombrados, copias y
plantilla precompilada sin encabezado O0, respectivamente en
A:/temp/nsc-t09-sysv-padded-pair-cd-db9g8guy,
A:/temp/nsc-t09-sysv-padded-pair-sd-h3a124xh,
A:/temp/nsc-t09-sysv-padded-pair-id-knesn7xg,
A:/temp/nsc-t09-sysv-padded-pair-dc-dhwkgmw6,
A:/temp/nsc-t09-sysv-padded-pair-ds-l80__ndl,
A:/temp/nsc-t09-sysv-padded-pair-vc-vkb7hqj0 y
A:/temp/nsc-t09-sysv-padded-pair-vs-myh0y28w.

La sonda y los metadatos pasaron, incluidos rechazos de records demasiado
pequenos para su segunda pieza (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Las regresiones del par previo, retorno
indirecto con vector, retorno `i24` y retorno mixto pasaron en
A:/temp/nsc-t09-sysv-return-pair-id-mixed-kgjdf_h8,
A:/temp/nsc-t09-sysv-class-vs-gp78z1av,
A:/temp/nsc-t09-sysv-return-one-i24-mixed-qzy2ny1j y
A:/temp/nsc-t09-sysv-return-record-mixed-9c3qnk_r. El control general conserva
el rechazo de un record empaquetado pasado indirectamente y de callback
(A:/temp/nsc-t09-sysv-prueba-06o6hx33). Otras disposiciones siguen pendientes;
T09-003 y la tanda 9 permanecen abiertos.

## Retorno directo de una pieza con argumentos mixtos

Tres funciones C devuelven records transportados como `i24`, `float` y vector
de dos `float`, y reciben un record directo mixto, un record `byval` y
escalares. Los controles C pasaron y NSC rechazaba cada firma antes del cambio:
A:/temp/nsc-t09-sysv-return-one-i24-mixed-gzr5pqek,
A:/temp/nsc-t09-sysv-return-one-float-mixed-pgsvn55f y
A:/temp/nsc-t09-sysv-return-one-vec-mixed-9epphoa8. Clang 23.1.0 confirmo las
piezas fisicas y el `byval` (A:/temp/t09-retorno-una-*-clang.ll).

El compilador conserva que el retorno logico es un record aun cuando su pieza
fisica coincide con un escalar, y lo reconstruye despues de la llamada. El
contrato vigente conserva la pieza observada. Las tres muestras pasaron ocho
comparaciones C/NS en estatica y DLL O0–O3, argumentos nombrados, comprobacion
de copia y plantilla precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-return-one-i24-mixed-_5ddfz14,
A:/temp/nsc-t09-sysv-return-one-float-mixed-nxj_fvzu y
A:/temp/nsc-t09-sysv-return-one-vec-mixed-qvqzx40y.

Se agregaron despues retornos de record `i32` e `i64`, este ultimo con un valor
fuera del rango de 32 bits. Clang confirmo ambas piezas y las muestras pasaron
las mismas comparaciones y precompilacion en
A:/temp/nsc-t09-sysv-return-one-i32-mixed-3tazhc6o y
A:/temp/nsc-t09-sysv-return-one-i64-mixed-p56s8jhw; no se afirma un rechazo
previo ejecutado de esas variantes. La sonda y los metadatos pasaron para las
cinco formas (A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log).

Pasaron las regresiones del retorno directo de dos piezas, record de tres bytes
previo, retorno escalar con records, retorno escalar `float` y control general:
A:/temp/nsc-t09-sysv-return-record-mixed-ox8n10d1,
A:/temp/nsc-t09-sysv-r3-z3coia2b,
A:/temp/nsc-t09-sysv-scalar-direct-records-vy1yjy88,
A:/temp/nsc-t09-sysv-return-float-mixed-k_w90kli y
A:/temp/nsc-t09-sysv-prueba-u4ugupfo. Otras firmas y clases permanecen
pendientes; T09-003 y la tanda 9 siguen abiertos.

## Retorno directo de dos piezas con argumentos mixtos

Una funcion C devuelve un record `double`/`i32` y recibe escalares, un record
directo mixto y un record diferente por `byval`. El control C paso y NSC
rechazaba la firma antes del cambio
(A:/temp/nsc-t09-sysv-return-record-mixed-o35xzxxb). Clang 23.1.0 confirma
retorno `{ double, i32 }`, piezas directas y un `byval` alineado a ocho
(A:/temp/t09-retorno-par-di-clang.ll).

NSC valida el plan de retorno y de argumentos, pasa cada copia conforme a Clang
y reconstruye el record a partir de las dos piezas devueltas. El contrato
vigente conserva los tipos fisicos del retorno y de los argumentos. La muestra
paso ocho comparaciones C/NS en estatica y DLL O0–O3, argumentos nombrados y
plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-return-record-mixed-uszhtcn4).

Se agregaron despues retornos `{ i64, double }` y dos vectores de dos `float`,
con los mismos argumentos y comprobacion de todos los campos. Clang confirmo
ambas clasificaciones (A:/temp/t09-retorno-par-id-clang.ll y
A:/temp/t09-retorno-par-vv-clang.ll); ambas muestras pasaron las mismas ocho
comparaciones y precompilacion en
A:/temp/nsc-t09-sysv-return-pair-id-mixed-z4njjxog y
A:/temp/nsc-t09-sysv-return-pair-vv-mixed-g2blcjyt. No se afirma que se haya
ejecutado un rechazo previo de estas dos variantes.

La sonda y la prueba de metadatos pasaron, incluidos el retorno vectorial y un retorno inconsistente
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron las
regresiones del retorno mixto previo, par entero, retorno indirecto con entero
estrecho, retorno escalar y control general:
A:/temp/nsc-t09-sysv-mixed-di-9lx4oz_z,
A:/temp/nsc-t09-sysv-pair-ints-vmro7rbd,
A:/temp/nsc-t09-sysv-sret-narrow-e8s5l5zn,
A:/temp/nsc-t09-sysv-scalar-mixed-is1jbj6l y
A:/temp/nsc-t09-sysv-prueba-xfwqkwjk. Otras clases y firmas quedan abiertas;
T09-003 y la tanda 9 no se consideran cerrados.

## Enumeraciones C con records SysV

Dos funciones reciben y devuelven un enum C de 32 bits junto a records entero y
vectorial. La primera utiliza un valor negativo y subyacente con signo; el
control C paso y NSC rechazo la firma antes del cambio
(A:/temp/nsc-t09-sysv-enum-signed-record-c4_dnj8a). El plan de Clang usa
`i32` para argumento y retorno (A:/temp/t09-enum-signed-record-clang.ll).

La muestra inicialmente propuesta como enum sin signo contenia el valor
`4000000000U`, pero una asercion revelo que Clang para este objetivo lo habia
convertido a un enum con signo (fallo reproducido en
A:/temp/nsc-t09-sysv-enum-unsigned-record-2invevq3). Se corrigio la muestra
declarando base `unsigned int` explicitamente, extension aceptada por Clang en
modo C11; su asercion comprueba signo y tamano, y la firma usa `i32`
(A:/temp/t09-enum-unsigned-record-clang.ll). No se afirma un rechazo previo
ejecutado para la muestra corregida.

NSC comprueba la anchura y el tipo subyacente observado y conserva ambos en el
contrato vigente. Las dos muestras pasaron ocho comparaciones C/NS en estatica
y DLL O0–O3, argumentos nombrados, aserciones de Clang y plantilla
precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-enum-signed-record-vixcqyha y
A:/temp/nsc-t09-sysv-enum-unsigned-record-e8dbicdb. La prueba de metadatos
comprueba ambos tipos subyacentes y rechaza uno ausente; paso junto a la sonda
Clang (A:/temp/t09-planes-identity.log y A:/temp/t09-planes-clang.log).

Pasaron las regresiones de records directos, puntero a record, record directo
de tres bytes con retorno indirecto y llamadas generales:
A:/temp/nsc-t09-sysv-scalar-direct-records-19soh0jz,
A:/temp/nsc-t09-sysv-recordp-record-37jjtm4q,
A:/temp/nsc-t09-sysv-direct3-indirect-bmi9s6a3 y
A:/temp/nsc-t09-sysv-prueba-zhqcwm05. Enum C++ y otras anchuras permanecen
pendientes; T09-003 y la tanda 9 siguen abiertos.

## Punteros a datos con records SysV

Dos funciones reciben un record entero y otro vectorial por valor junto a un
puntero, que devuelven tras escribir en el objeto apuntado. Las muestras usan
un puntero a record C y un puntero a `short`. Los controles C pasaron y NSC las
rechazo antes del cambio en A:/temp/nsc-t09-sysv-recordp-record-0lwmr6de y
A:/temp/nsc-t09-sysv-shortp-record-uo9u2hof. Clang 23.1.0 representa el
puntero a record como `ptr` en argumento y retorno
(A:/temp/t09-recordp-record-clang.ll).

El importador admite los punteros a datos mapeados cuando el plan conserva esa
forma fisica y el generador pasa la direccion directamente. Los punteros a
funcion se mantienen fuera de esta ruta. Ambas muestras pasaron ocho
comparaciones C/NS con estatica y DLL en O0–O3, llamadas nombradas y plantilla
precompilada sin encabezado O0, verificando direccion y escritura:
A:/temp/nsc-t09-sysv-recordp-record-v_7ggzva y
A:/temp/nsc-t09-sysv-shortp-record-euo77f7r. Pasaron las regresiones de
booleano, puntero a `int` y control general, que conserva el rechazo de la
conversion a callback: A:/temp/nsc-t09-sysv-bool-record-pt39wwep,
A:/temp/nsc-t09-sysv-intp-record-873rcnzm y
A:/temp/nsc-t09-sysv-prueba-0mhi34vk. No se acreditan todos los tipos de
puntero ni callbacks; T09-003 y la tanda 9 siguen abiertos.

## Booleanos y punteros con records SysV

Cinco funciones reciben dos records directos, escalares y respectivamente
`_Bool`, `int*`, `double*`, `void*` o `int**`, y devuelven el mismo tipo. Los
controles C pasaron y NSC rechazo las firmas antes del cambio:
A:/temp/nsc-t09-sysv-bool-record-flo1seol,
A:/temp/nsc-t09-sysv-intp-record-xnscg9lv,
A:/temp/nsc-t09-sysv-doublep-record-z3uczcr7,
A:/temp/nsc-t09-sysv-voidp-record-sg1jp12q y
A:/temp/nsc-t09-sysv-intpp-record-24ltvjjn. Clang 23.1.0 uso `i1 zeroext`
para el booleano y `ptr` para los punteros, tanto en argumento como retorno
(archivos A:/temp/t09-*-record-clang.ll).

NSC conserva la distincion entre el tamano semantico booleano y la pieza
fisica, aplica `zeroext` en declaracion y llamada y reconstruye el plan en el
contrato vigente. Cada muestra paso ocho comparaciones C/NS, estatica y DLL
O0–O3, argumentos nombrados y plantilla precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-bool-record-ykvkgx9f,
A:/temp/nsc-t09-sysv-intp-record-5dkzy2mq,
A:/temp/nsc-t09-sysv-doublep-record-m86v3nh8,
A:/temp/nsc-t09-sysv-voidp-record-jc3snkx2 y
A:/temp/nsc-t09-sysv-intpp-record-yl7mazxm. Las pruebas verifican ambos valores
booleanos, identidad de punteros y, donde corresponde, escrituras indirectas.

La sonda y el control de metadatos pasaron (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Pasaron tambien las regresiones de extensiones
enteras combinadas, retorno indirecto y llamadas generales:
A:/temp/nsc-t09-sysv-narrow-mixed-record-tiuhp8ak,
A:/temp/nsc-t09-sysv-sret-narrow-tji6bu_7 y
A:/temp/nsc-t09-sysv-prueba-xqeqpls0. Estas muestras no certifican todos los
punteros, callbacks o combinaciones de firmas; T09-003 y la tanda 9 siguen
abiertos.

## Enteros estrechos con records SysV

Cuatro funciones reciben y devuelven respectivamente `sbyte`, `byte`, `short`
y `ushort`, junto a un record entero, un record vectorial y escalares. Los
controles C pasaron y NSC rechazaba las firmas antes del cambio:
A:/temp/nsc-t09-sysv-narrow-sbyte-record-lnljui14,
A:/temp/nsc-t09-sysv-narrow-byte-record-rfe048r3,
A:/temp/nsc-t09-sysv-narrow-short-record-qzkri88t y
A:/temp/nsc-t09-sysv-narrow-ushort-record-9c203rou. Clang 23.1.0 confirmo
`signext` para los tipos con signo y `zeroext` para los tipos sin signo, tanto
en el retorno como en el argumento (A:/temp/t09-estrecho-*-record-clang.ll).

NSC valida que las extensiones logicas y fisicas coincidan con el tipo C y
aplica los atributos en declaracion y llamada; el contrato vigente reconstruye
la misma extension. Cada muestra paso ocho comparaciones C/NS en estatica y DLL
O0–O3, argumentos nombrados, comprobacion de atributos LLVM y plantilla
precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-narrow-sbyte-record-9zyx7_z6,
A:/temp/nsc-t09-sysv-narrow-byte-record-ybsde98d,
A:/temp/nsc-t09-sysv-narrow-short-record-2h69be8p y
A:/temp/nsc-t09-sysv-narrow-ushort-record-tzt6aaz5.

Una llamada con `sbyte` y `ushort` simultaneos paso las mismas comprobaciones
(A:/temp/nsc-t09-sysv-narrow-mixed-record-zd0j3eye), al igual que una llamada
con retorno indirecto `sret` y argumento `sbyte`
(A:/temp/nsc-t09-sysv-sret-narrow-swejnl_g). Esas dos variantes se agregaron
despues de implementar la forma y no se afirma un rechazo previo ejecutado.
La sonda Clang y metadatos, incluidos planes con extension inconsistente,
pasaron (A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log).
Las regresiones de records directos, records mixtos, retorno indirecto y
llamadas escalares pasaron en
A:/temp/nsc-t09-sysv-scalar-direct-records-4a1dsq6r,
A:/temp/nsc-t09-sysv-scalar-mixed-xtn291g_,
A:/temp/nsc-t09-sysv-sret-mixed-v4wh5tj1 y
A:/temp/nsc-t09-sysv-prueba-rd5f10eg. Otras firmas y tipos permanecen
pendientes; T09-003 y la tanda 9 siguen abiertos.

## Retorno escalar con records directos de una pieza

Una funcion C devuelve `int` y recibe dos records directos de una pieza,
intercalados con escalares: el primero contiene un `int` y el segundo dos
`float`. Los controles C pasaron y NSC rechazo la firma en estatica y DLL antes
del cambio (A:/temp/nsc-t09-sysv-scalar-direct-records-bioop6l_). Clang
23.1.0 confirmo las piezas `i32` y `<2 x float>`
(A:/temp/t09-escalar-records-directos-clang.ll).

Una pieza `i32` puede representar un escalar o un record; el plan fisico por si
solo no permite distinguirlos. HIR conserva la presencia semantica de records
y la transmite a MIR para seleccionar el generador adecuado sin cambiar las
llamadas exclusivamente escalares. El contrato vigente conserva la pieza
vectorial para consumo precompilado.

La prueba paso ocho comparaciones C/NS en estatica y DLL O0–O3, argumentos
nombrados y plantilla precompilada sin encabezado O0; comprueba ambos elementos
del vector y que las copias originales no cambien
(A:/temp/nsc-t09-sysv-scalar-direct-records-fcnv2xs1). Pasaron la sonda y los
metadatos, incluidos controles de piezas enteras y vectoriales
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron las
regresiones de llamada mixta con `byval`, retorno de record de tres bytes y
llamadas escalares, incluidas sus pruebas de precompilacion y rechazos:
A:/temp/nsc-t09-sysv-scalar-mixed-s8hwmdir,
A:/temp/nsc-t09-sysv-r3-j4ectv54 y A:/temp/nsc-t09-sysv-prueba-1raha6lv.
La muestra no certifica todas las firmas, retornos ni layouts; T09-003 y la
tanda 9 siguen abiertos.

## Retorno escalar con records directos e indirectos

Se contrasto una llamada que devuelve `int` y recibe `float`, un record de
`double` e `int`, un record de cuatro enteros de 64 bits y otro `int`. El
control C funciono en estatica y DLL mientras NSC rechazaba la firma
(A:/temp/nsc-t09-sysv-scalar-mixed-d1g80255). Clang 23.1.0 confirmo retorno
`i32`, piezas `double`/`i32` y argumento `byval` alineado a ocho
(A:/temp/t09-scalar-mixed-clang.ll).

NSC transporta el plan de argumentos y el retorno directo por HIR, MIR y LLVM;
el contrato vigente conserva las piezas observadas y reconstruye el argumento
indirecto. La muestra paso ocho comparaciones C/NS en estatica y DLL O0–O3,
argumentos nombrados y plantilla precompilada sin encabezado O0. El productor
altera las copias y el consumidor verifica los valores originales. La prueba
se reforzo para usar los cuatro campos del record indirecto y paso de nuevo
(A:/temp/nsc-t09-sysv-scalar-mixed-7xu1jv3b). La sonda y prueba de metadatos
pasaron (A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log).
Pasaron las regresiones de retorno indirecto con piezas vectoriales, retorno
indirecto sin `byval` y el control general con sus rechazos:
A:/temp/nsc-t09-sysv-class-vf-gsbrspu9,
A:/temp/nsc-t09-sysv-sret-mixed-daoybwim y
A:/temp/nsc-t09-sysv-prueba-4t9v5m_m.

Esa ampliacion inicial cubria retorno `int` de 32 bits y planes con al menos un
record indirecto o descompuesto; otras formas seguian pendientes.

Una ampliacion posterior cubre la misma combinacion de argumentos con retorno
`long`, `uint`, `ulong`, `float`, `double` y `void`; este ultimo comprueba su
resultado mediante un puntero de salida. Clang 23.1.0 emitio respectivamente
`i64`, `i32`, `i64`, `float`, `double` y `void`, conservando el argumento
`byval` (archivos A:/temp/t09-retorno-*-clang.ll). Los controles C de las cinco
formas distintas de `ulong` pasaron y NSC las rechazaba antes de la ampliacion:
A:/temp/nsc-t09-sysv-return-long-mixed-xogmiivs,
A:/temp/nsc-t09-sysv-return-uint-mixed-s4s3aakk,
A:/temp/nsc-t09-sysv-return-float-mixed-6gj8mug3,
A:/temp/nsc-t09-sysv-return-double-mixed-61yetu8n y
A:/temp/nsc-t09-sysv-return-void-mixed-rwmy_nsy. La muestra `ulong` se agrego
despues de habilitar esa clasificacion; no se afirma un rechazo previo suyo.

Cada una de las seis muestras paso ocho comparaciones C/NS, estatica y DLL en
O0–O3, argumentos nombrados y una plantilla precompilada sin encabezado O0:
A:/temp/nsc-t09-sysv-return-long-mixed-d7s75l2l,
A:/temp/nsc-t09-sysv-return-uint-mixed-r_klx09m,
A:/temp/nsc-t09-sysv-return-ulong-mixed-kmd6dh1k,
A:/temp/nsc-t09-sysv-return-float-mixed-5n627f5c,
A:/temp/nsc-t09-sysv-return-double-mixed-lfja7pr4 y
A:/temp/nsc-t09-sysv-return-void-mixed-ir6k1pvt. Los retornos enteros usan
valores fuera del rango de 32 bits cuando corresponde. La sonda Clang y los
metadatos pasaron (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). Las regresiones de `int`, retorno indirecto
y control general pasaron en A:/temp/nsc-t09-sysv-scalar-mixed-rol8e20p,
A:/temp/nsc-t09-sysv-sret-mixed-j4g82clv y
A:/temp/nsc-t09-sysv-prueba-zavu8jpq. Siguen pendientes otras clases, firmas,
retornos y callbacks; T09-003 y la tanda 9 permanecen abiertos.

## Record con `double` y dos enteros con retorno indirecto

El control C de una funcion que recibe `float`, un record con `double` y dos
`int` y un `int`, y devuelve un record indirecto de 20 bytes, paso con enlace
estatico y DLL mientras NSC rechazaba la firma
(A:/temp/nsc-t09-sysv-sret-double-i64-l9eclpay). Clang 23.1.0 clasifico el
record en las piezas `double` e `i64`
(A:/temp/t09-sret-double-i64-clang.ll). NSC admite esa forma y conserva ambas
piezas en el contrato vigente.

La prueba paso ocho comparaciones C/NS con biblioteca estatica y DLL O0–O3,
llamadas normales y nombradas y plantilla precompilada sin encabezado O0;
comprueba los dos campos enteros y que la copia original permanezca intacta
(A:/temp/nsc-t09-sysv-sret-double-i64-cbzgjcbu). La sonda Clang y la prueba de
metadatos pasaron (A:/temp/t09-planes-clang.log y
A:/temp/t09-planes-identity.log). La regresion del record `double`/`i32` paso
en A:/temp/nsc-t09-sysv-sret-mixed-atyyx9oh. La regresion general paso y conserva el
rechazo de una firma diferente con `double` e `i16` y de callback
(A:/temp/nsc-t09-sysv-prueba-12rcdydv). Otras firmas y clases permanecen
pendientes; T09-003 y la tanda 9 siguen abiertos.

## Record de una pieza flotante con retorno indirecto

Tres muestras reciben un record directo, respectivamente de un `float`, un
`double` o dos `float`, junto a escalares y con retorno indirecto de 20 bytes.
Clang clasifica los records como `float`, `double` y `<2 x float>`. Los
controles C pasaron y NSC rechazo cada firma antes del cambio en
A:/temp/nsc-t09-sysv-sret-one-f-uzpn58fi,
A:/temp/nsc-t09-sysv-sret-one-d-lyqgpvlk y
A:/temp/nsc-t09-sysv-sret-one-v-fas0cow5.

NSC conserva la pieza observada por Clang en el contrato vigente, la carga
desde la copia del record y la pasa con su tipo fisico. Cada muestra paso ocho
comparaciones C/NS, con biblioteca estatica y DLL en O0–O3, llamadas normales
y nombradas, comprobacion de firma LLVM y plantilla precompilada sin encabezado
en O0. Evidencia, en el mismo orden:
A:/temp/nsc-t09-sysv-sret-one-f-ewz0m4oi,
A:/temp/nsc-t09-sysv-sret-one-d-rajp96it y
A:/temp/nsc-t09-sysv-sret-one-v-fkvrw1pf.

La sonda de Clang y la prueba de metadatos pasaron
(A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log). Pasaron las
regresiones del record mixto sin `byval`, del vector/float con `byval` y del
control general con rechazo de una forma aun no admitida y de callback:
A:/temp/nsc-t09-sysv-sret-mixed-n3zd0my4,
A:/temp/nsc-t09-sysv-class-vf-__cjdsj1 y
A:/temp/nsc-t09-sysv-prueba-8myjsk_9. La muestra negativa usa ahora piezas
`double` e `i64`. Estas pruebas no acreditan todas las firmas o clases ABI;
T09-003 y la tanda 9 siguen abiertos.

## Piezas flotante y entera con retorno indirecto

El control C de una firma con retorno indirecto, un record directo clasificado
como `double`/`i32` y otro argumento `byval` paso, y NSC rechazaba la firma
(A:/temp/nsc-t09-sysv-mixedpair-indirect-20v06i3w). La implementacion valida
esa clasificacion y conserva explicitamente las clases fisicas observadas por
Clang en el contrato vigente; no intenta deducirlas solo del tamano del tipo.

La muestra paso ocho comparaciones estatica/DLL O0–O3, argumentos nombrados y
plantilla precompilada sin encabezado O0
(A:/temp/nsc-t09-sysv-mixedpair-indirect-infboyde). La sonda de Clang y el
control de metadatos comprobaron las piezas y rechazaron una clasificacion
inconsistente (A:/temp/t09-planes-clang.log y A:/temp/t09-planes-identity.log).
Pasaron las regresiones de dos piezas enteras con retorno indirecto, de
alineaciones indirectas distintas y el control general con rechazo de una
clase vectorial aun pendiente (A:/temp/nsc-t09-sysv-directpair-indirect-4uetfmjr,
A:/temp/nsc-t09-sysv-indirect-diff16-6bz2kcxr y
A:/temp/nsc-t09-sysv-prueba-ewdl26ki). La muestra cubre el orden
`double`/`i32`; otras clases y firmas siguen pendientes. T09-003 permanece
abierto.

