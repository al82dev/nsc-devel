# T09-003: primer caso de convencion SysV en Windows x64

Estado: en implementacion; el item T09-003 sigue abierto. Clang oficial 23.1.0
acepta `__attribute__((sysv_abi))` en una biblioteca C para Windows x64. Antes
del cambio, el control C enlazo y ejecuto una suma, mientras NSC rechazo la
convencion en HIR. Reproduccion: A:/temp/nsc-t09-sysv-77xbbf6_.

El primer cambio admite llamadas directas no variadicas con parametros `int`,
`uint`, `long`, `ulong`, `sbyte`, `byte`, `short`, `ushort`, `bool`, `float`,
`double` o `int*` y retorno de esos tipos o `void`. La convencion se transporta de HIR
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

No se afirma compatibilidad de SysV con agregados, otros escalares o punteros,
variadicas, punteros de funcion, aislamiento TLS ni otros destinos. No se han cambiado
Clang, NSLink ni el contrato de NSMeta.
