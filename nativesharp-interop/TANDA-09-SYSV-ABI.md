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
