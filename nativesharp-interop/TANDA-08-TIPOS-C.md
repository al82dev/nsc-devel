# Tanda 08: declaraciones, nombres y tipos C

Estado: auditoría inicial; ningún ítem se cierra solo por observar datos en Clang.

## Base de sintaxis para typedefs con apuntado volatile

Se preparó un dato de volatile inmediato en la sintaxis de tipos y se conserva
en sus rutas de clonación. Ningún alias lo activa todavía: faltan caché,
identidad, proyección y consumidores. El cambio no se ha construido ni validado.
Se preparó además la persistencia del dato y el incremento de esquemas de
caché genérica y sintáctica; falta comprobar su restauración.
Se prepararon comprobaciones de clonación y de ida y vuelta del dato junto con
restrict. No se han ejecutado todavía. La batería actual incluye la prueba de
ida y vuelta, pero el target de componentes que contiene la prueba de clonación
no aparece en la configuración incremental ON; incorporarlo o verificarlo en
una configuración habilitada antes de afirmar esa cobertura.
El patrón genérico y el volcado de sintaxis preparan la distinción volatile;
esto no cambia por sí solo la identidad ABI ni activa un typedef.
También se preparó la lectura del dato en declaraciones de parámetros,
miembros y adaptadores de función; falta conectar los aliases y validar.
Los locales tipados preparan la preservación del dato y su comprobación en
inicializadores. Ningún typedef lo activa todavía.
Se preparó la misma conservación para campos y respaldos automáticos; faltan
pruebas de extremo a extremo y activación del alias.

## Calificador volatile del objeto apuntado: base en preparación

Los párrafos siguientes registran la secuencia de cambios; las barreras que se
describen como activas corresponden al momento de cada preparación. El estado
posterior es la proyección limitada del global C escalar descrita más abajo,
todavía sin validar y con su auditoría de escapes pendiente.

Se agrega al resultado semántico y al registro de expresiones un dato separado
del volatile del almacenamiento del puntero. Por ahora se inicia en falso y
ninguna declaración adicional queda habilitada; falta conectar productores,
conversiones, consumidores, accesos y pruebas. El cambio aún no fue construido.
Se preparó el transporte desde la descripción de globales importados hasta el
resultado semántico. La barrera de importación sigue activa y no se demuestra
todavía uso de punteros a volatile.
La desreferencia y el índice preparan la propagación del calificador al
almacenamiento resultante. Falta conectar HIR, conversiones y validación.
HIR prepara la marca volatile de la carga y de la dirección del objeto
apuntado; no se ha activado ni comprobado la ruta.
El acceso a un campo también prepara la propagación desde una dirección
desreferenciada ya calificada, sin asumir que el puntero lo esté. Aún falta
habilitar y validar el apuntado a struct volatile.
Se preparó además la conservación en locales inferidos y el rechazo de una
inicialización explícita que descartaría el calificador; falta cubrir
asignaciones y los demás recorridos antes de habilitar la importación.
También se preparó la propagación en operaciones aritméticas de punteros,
coalescencia y condicionales; permanece sin ejecutar.
Se preparó el rechazo de asignaciones que perderían el volatile inmediato;
la ruta de entrada continúa cerrada.
Las conversiones a puntero o entero también quedan rechazadas de forma
provisional si perderían el calificador; aún no se ejecutó la validación.
Las rutas ordinarias de resolución de llamadas y constructores preparan un
rechazo temporal de argumentos con apuntado volatile. No es soporte final:
faltan contratos por parámetro y auditoría de llamadas indirectas y variádicas.
Se preparó un dato de contrato volatile para parámetros y su serialización;
el esquema de snapshot cambia a v2 para no leer con el formato anterior.
La compatibilidad por reanálisis de snapshots anteriores debe verificarse.
La resolución ordinaria prepara la aceptación sólo cuando el parámetro también
conserva volatile; las vías indirectas y variádicas siguen sin auditar.
La suite prepara una prueba con funciones C proyectadas para la aceptación de
un parámetro volatile y el rechazo de otro sin ese calificador; sin ejecutar.
Se añadió una barrera provisional en las rutas de llamadas indirectas y
variádicas cuyos tipos no expresan el contrato. No equivale a soporte de
callbacks o variádicos con volatile, que siguen pendientes.
El fixture mantiene controles para punteros anidados volatile; inicialmente
también rechazaba el resultado escalar, que se conectó posteriormente.
Se preparó el transporte del calificador de resultado desde el tipo de Clang
hasta el resultado de la llamada y su snapshot. En ese momento la proyección de
resultados volatile permanecía rechazada. El esquema se
incrementó a v3 para diferenciar este formato del v2 preparado anteriormente.

Se conectó la proyección de un global C de un nivel a escalar volatile y se
preparó su caso de lectura y escritura. La compilación, ejecución y auditoría
de escapes siguen pendientes; no equivale a soporte general de punteros
calificados. Se prepararon comprobaciones de IR y rechazos de pérdida del
calificador en inicialización y llamadas.
El inventario inicial también prepara la proyección con el modo C, aun sin
lookup; la comprobación correspondiente está pendiente de ejecución.
También se prepararon casos negativos de escape por tupla y conversión
explícita; esperan ejecución y no completan la auditoría de escapes.
Se preparó una barrera específica al tomar la dirección de un local cuyo
apuntado es volatile: un doble puntero todavía perdería el calificador interno.
Se aplicó la misma protección al parámetro por valor que porta ese contrato.
El mismo fixture incluye lectura del apuntado const volatile y rechazo de
escritura; estas comprobaciones esperan ejecución.
Se conectó además el apuntado inmediato a struct C con layout importado y se
prepararon accesos a campo y rechazo de dirección sin calificar. La copia y
otras operaciones sobre el agregado completo no están verificadas.
También se conectó la proyección de puntero a enum C volatile con identidad y
layout resueltos, y se preparó su ejecución mixta; sigue sin validar.
El mismo caso prepara el paso del puntero a parámetros C volatile y const
volatile; todavía no hay resultado de ejecución.
La búsqueda de IR preparada comprueba la presencia de una carga volatile de
enum, pero por sí sola no identifica que provenga de este puntero.
El fixture prepara también acceso a un campo puntero dentro de un struct
volatile apuntado, para comprobar que el dato final no hereda volatile.
Se añadió el caso opuesto: puntero const a dato volatile, con escritura
permitida en el dato y rechazo de reasignación del puntero. Sin ejecutar.
Se conectó además el resultado C de puntero a escalar volatile con el
metadato de Clang y se preparó su prueba mixta. También se prepararon
resultados C a struct y enum volatile con tipos importados; los resultados
anidados siguen diferidos. Ninguna de estas rutas se ha validado todavía.
La suite prepara controles separados para que firmas C++ con apuntado volatile
no se proyecten antes de resolver sus contratos y sobrecargas.
Se preparó asimismo el rechazo provisional al retornar ese puntero mediante
un tipo que no representa volatile; no está ejecutado.
Se preparó el transporte en identificadores de parámetros y el rechazo
provisional de inicialización de campos que descartaría el calificador.
Se preparó además la conservación del calificador en resultados de move para
locales, parámetros, campos, elementos e iteradores; aún no se ha validado.
Las tuplas aún no representan calificadores por elemento; se preparó una
barrera provisional para evitar pérdida silenciosa, pendiente de validar.
También se preparó la barrera equivalente para inicializadores de arreglos
nativos; no cubre todavía todos los agregados ni sustituye su implementación.
Una prueba interna preparada inyecta un global con símbolo de Clang para
ejercitar el acceso, el IR y los rechazos de inicialización, asignación y
retorno. No habilita la importación real y aún no se ha ejecutado.
La prueba prepara también un local transferido mediante move y exige que su IR
mantenga las cargas y escrituras volatile.

## Parámetros C puntero a escalar volatile: implementación pendiente de validar

La proyección permite preparar una llamada a una función con enlace C cuyo
parámetro es un puntero de un nivel a un escalar volatile. La función C realiza
los accesos calificados; NSC conserva el tipo original de Clang y const del
apuntado. El fixture prepara ejecución mixta y rechazo de pérdida de const.
La construcción y validación agrupada están pendientes; no se afirma soporte
hasta disponer de resultados. Los retornos, globales, callbacks, punteros
anidados y la resolución de sobrecargas C++ no se habilitan con este cambio.

La misma proyección se preparó para un puntero de un nivel a un struct C
reconocido por el inventario. La prueba de ejecución conjunta queda pendiente.
Se preparó además el caso equivalente para enum C, también pendiente de
construcción y ejecución; un caso negativo exige rechazo cuando la llamada
descartaría const del apuntado.
También se preparó volatile void* como parámetro C, con lectura dentro de C.

El fixture de escalares y structs incluye una comprobación específica del IR C
para exigir accesos volatile dentro de cada función correspondiente. La
comprobación preparada está pendiente de ejecución.

## Optimización del proyecto mixto de records C: 2026-11-06

Con `bb85ea5e2` y `6cbe3f99a`, `call nativesharp-interop\validar-tanda-08.cmd`
terminó con código 0: pasaron ON/OFF, suite Clang, restauración, serialización y
los siete proyectos. El proyecto `record-c` compiló y ejecutó los mismos casos
con NSC O0, O1, O2 y O3 contra la misma biblioteca C en Windows x64; las
comprobaciones IR y negativas siguen ejecutándose en O0.

Esto no demuestra todas las combinaciones de tipos, otras arquitecturas ni
otras configuraciones del compilador C. Los ítems generales de la tanda 08
permanecen abiertos.

## Requisito TLS de biblioteca C: 2026-11-06

El fixture `pruebas-tanda-08-tls-c` compiló con Clang para Windows x64, pero
no llegó a ejecutarse. El primer enlace falló por `__imp_GetExitCodeThread`,
dependencia de la prueba; `fb3f5d604` la eliminó usando memoria compartida tras
esperar al hilo. El segundo enlace falló en NSLink con `NSL1020` para
`_tls_index`, referenciado por el objeto C.

La inspección del objeto muestra sección `.tls$` y relocaciones AMD64 SECREL.
La fuente consultada de NSLink reconoce SECREL, pero esta observación no prueba
que el ejecutable final construya o inicialice el directorio TLS. No se modificó
NSLink ni se habilitó la importación del global TLS. La independencia entre
hilos permanece sin verificar; T08-004 y T08-015 siguen abiertos.

## Resultado C de puntero a const: 2026-11-06

Con `a06de3940`, `b199dd0d7` y `fd32bd2fc`, la validación agrupada terminó con
código 0: pasaron ON/OFF, la suite Clang, restauración, serialización y los
siete proyectos preparados. La primera ejecución había fallado porque la suite
esperaba que un retorno `const int*` fuera rechazado; se actualizó para exigir
la proyección y conservación del tipo de Clang, manteniendo otros rechazos.

En Windows x64 O0 se leyeron resultados `const int*` y `const struct*`, y el
segundo se pasó a una función C compatible. Se rechazaron escrituras, conversión
mutable y llamada a función mutable, con los diagnósticos comprobados por el
fixture. No se cubren todos los tipos, niveles, retornos C++ ni punteros a
volatile; T08-014 y T08-015 continúan abiertos.

## Puntero global C a struct const: 2026-11-06

Con `d6fe4cb64`, `1425ee99e`, `71503d94a` y `8dfdc278c`, la validación
agrupada terminó con código 0: pasaron ON/OFF, la suite Clang, restauración,
serialización y los siete proyectos. El fixture Windows x64 O0 leyó el campo
apuntado y lo pasó a una función C que acepta `const struct*`; también asignó
un puntero mutable al global de apuntado const.

Se rechazaron la escritura al campo, su dirección mutable, la conversión a
puntero mutable y la llamada a una función que requiere `struct*`. Después,
`e4025eb6f` añadió el control positivo de esa misma función con puntero mutable;
el fixture actualizado se ejecutó con el compilador ya construido y terminó con
código 0. No se demuestran niveles adicionales ni punteros a volatile; T08-014
y T08-015 continúan abiertos.

## Matrices y records C anidados calificados: 2026-11-06

Con `4e5c8fbfa`, `44f51e8e4`, `1706dfdc8`, `57d2da108`, `73b8896f3` y
`fb021f5c8`, la validación agrupada terminó con código 0: pasaron las
construcciones incrementales ON/OFF, la suite Clang, restauración,
serialización y los siete proyectos preparados.

El fixture Windows x64 O0 leyó una matriz de structs const y escribió y leyó
una matriz volatile, contrastando con C y con instrucciones volatile en IR.
Comprobó un campo anidado volatile y un campo puntero: la carga del puntero es
volatile y la escritura en el objeto apuntado es ordinaria. Se rechazaron la
escritura const y las direcciones sin calificador con diagnósticos específicos.

La primera ejecución detectó una escritura apuntada incorrectamente volatile;
`57d2da108` separó la dirección de destino de la carga del puntero. Estas
muestras no demuestran todos los tipos, calificadores o combinaciones; los
ítems generales de la tanda 08 siguen abiertos.

## Global C volatile struct: 2026-11-06

Con `3bc7897e1`, `f6a2b98ea`, `e0b846d90`, `e539befce`, `d79d793e1` y
`eb7f93bee`, la validación agrupada terminó con código 0. Pasaron ON/OFF, la
suite de Clang, restauración y serialización, y los siete proyectos preparados.

En Windows x64 O0, NativeSharp escribió y leyó un campo de un global C
`volatile struct`, contrastado con C. El IR contiene carga y escritura
`volatile` dirigidas a ese global. La dirección del campo como puntero sin
calificar fue rechazada con diagnóstico específico. En el caso separado de un
puntero global volatile, el IR muestra una carga volatile del puntero y una
escritura ordinaria en el objeto apuntado.

Esta evidencia cubre los fixtures descritos; no demuestra todos los campos,
anidamientos, arreglos de records calificados, punteros a volatile, TLS ni
configuraciones. T08-015 continúa abierto.

## Global C const struct: 2026-11-06

Con `0c9a80722`, `66de02635` y `d74f40926`, la validación agrupada terminó con
código 0. Pasaron la construcción incremental ON/OFF, la suite Clang, las
pruebas de restauración y serialización y los siete proyectos preparados.
NativeSharp leyó un campo de un global C `const struct`; se rechazaron la
escritura y la obtención de su dirección como `int*`.

La evidencia es Windows x64 O0 para el fixture señalado. El struct global
`volatile`, los arreglos y punteros a records calificados y otros contratos
siguen pendientes; T08-015 no está cerrado.

## Ampliación de globales C de record: 2026-11-06

Con `25f0c00fe` y `e4efafa23`, la validación agrupada terminó con código 0:
ON/OFF, sesión Clang, restauración, serialización y los siete proyectos pasaron.
El fixture de Windows x64 O0 comprobó también una matriz de structs, una union
global y un doble puntero, compartiendo los datos con una función C.

Los globales `const struct` y `volatile struct` se rechazaron con
`C/C++ global type is not yet mapped`; esto es una limitación pendiente, no
soporte de calificadores. Siguen abiertas las tareas generales de records,
globales, arreglos, punteros y calificadores.

## Validación agrupada de globales C de struct: 2026-11-06

Tras `96b857499`, `9f9ba2b64` y `a4a0b2aad`, la batería agrupada del
2026-11-05 terminó con código 1. La construcción ON/OFF, la suite Clang y los
demás proyectos pasaron, pero el proyecto `record-c` falló con
`Stage 40 cannot map named value type to LLVM carrier: type:CRecordPointer::GlobalOnly`.

`ad312479d` añadió la declaración física del almacenamiento externo usando el
layout observado. La repetición de `call nativesharp-interop\validar-tanda-08.cmd`
terminó con código 0: pasaron las construcciones incrementales ON/OFF, la suite
Clang, restauración, serialización y los siete proyectos preparados.

En Windows x64 O0, NativeSharp escribió un campo de un struct global C, un campo
mediante un puntero global y otro en un arreglo global de structs; una función C
comprobó los tres valores. El nombre del struct no aparece en código NativeSharp
ni en las firmas invocadas. Esto no demuestra copia, construcción o paso por valor,
calificadores de record, matrices de records ni todas las colisiones. T08-004,
T08-010, T08-015 y T08-016 siguen abiertos.

## Validación agrupada de arreglos globales volatile: 2026-11-05

Con `017e6c5d1`, `e274bd1f2`, `f23fb09bc`, `262d0d217` y `ba1acfc2f`,
`call nativesharp-interop\validar-tanda-08.cmd` terminó con código 0. Pasaron
las construcciones incrementales ON/OFF, la suite Clang, las pruebas de
restauración y serialización y los siete proyectos preparados.

En Windows x64 O0 se escribieron y leyeron arreglos y matrices globales C de
escalares y un arreglo de enum `volatile`; los scripts comprobaron las cargas
y escrituras `volatile` de los símbolos correspondientes en LLVM IR. Los
casos negativos rechazaron con diagnóstico específico la dirección de elementos
de arreglo y matriz `volatile`; la dirección de un elemento mutable sí funcionó.

La primera prueba negativa había demostrado que se aceptaba un `int*` que perdía
el calificador. El rechazo es una protección provisional, no soporte de punteros
calificados. Las muestras no cubren todos los tipos, niveles de calificación,
TLS o configuraciones; T08-015 y T08-016 siguen abiertos.

## Validación agrupada de arreglos globales const: 2026-11-05

Con `9d01ae4dc`, `5620f689d`, `851b324ae` y `d1e08c811`, la ejecución de
`call nativesharp-interop\validar-tanda-08.cmd` terminó con código 0. Pasaron
ON/OFF, suite Clang, restauración y equivalencia de serialización y los siete
proyectos preparados. Se leyeron arreglos globales `const int` y `const enum`.

Las compilaciones negativas rechazaron la escritura de elementos con `left
side of assignment is not assignable`. La conversión de la dirección de un
elemento a `int*` se rechazó por descartar `pointee constness`, según
`const-array-address.log`. Son casos Windows x64 O0: no cubren arreglos de
punteros, `volatile`, todas las conversiones ni todos los calificadores.
T08-004, T08-015 y T08-016 siguen abiertos.

## Validación agrupada de arreglos globales enum: 2026-11-05

Con `27f11ff19` y `39fc011f7`, `call nativesharp-interop\validar-tanda-08.cmd`
terminó con código 0. Pasaron la construcción incremental ON/OFF, la suite de
sesión Clang, las pruebas de serialización y restauración y los siete proyectos.
NativeSharp copió elementos de un arreglo y una matriz globales de enums C;
una función C comprobó los valores. Los tipos enum se seleccionaron sin
nombrarlos en NativeSharp ni utilizarlos en las firmas invocadas.

La evidencia corresponde a arreglos fijos sin calificar en Windows x64 O0.
No demuestra elementos calificados, record, punteros a enum ni otras variantes;
T08-004, T08-009 y T08-016 permanecen abiertos.

## Validación agrupada de arreglos globales C: 2026-11-05

La primera ejecución después de `7ca9b4533`, `c919ab94d` y `482d14f21`
comprobó el acceso a elementos, pero falló porque la asignación del arreglo
completo se compiló. `120f0423f` corrigió la asignabilidad del estado externo
sin cambiar la mutabilidad de los elementos.

La segunda ejecución de `call nativesharp-interop\validar-tanda-08.cmd`
terminó con código 0: pasaron ON/OFF, suite Clang, restauración, equivalencia y
los siete proyectos. NativeSharp escribió y leyó un arreglo `int[3]` y una
matriz `int[2][3]`; C comprobó los mismos valores. `array-assign.log` confirmó
el rechazo `left side of assignment is not assignable` para el arreglo entero.

El resultado corresponde a Windows x64 O0, escalares sin calificar y los
fixtures indicados. No demuestra globales de enum o record en arreglos,
calificadores, límites incompletos ni todas las dimensiones; T08-004 y T08-016
permanecen abiertos.

## Validación agrupada de punteros y volatile enum: 2026-11-05

Con `87aa55ced`, `1f3c9c6cb`, `2deedabf2` y `84a75735f`, la ejecución de
`call nativesharp-interop\validar-tanda-08.cmd` terminó con código 0. Pasaron
la construcción incremental ON/OFF, la suite Clang, restauración y equivalencia
de serialización y los siete proyectos preparados.

El proyecto mixto leyó punteros globales de uno y dos niveles a enums que no
aparecen en firmas ni nombres escritos en NativeSharp. También escribió y leyó
un global `volatile enum`; el script verificó operaciones `load volatile` y
`store volatile` referidas a `@volatile_mood` en el LLVM IR, además de la
ejecución. El resultado se limita a Windows x64 O0 y a los fixtures descritos;
no cubre punteros a volatile, TLS, todas las colisiones ni las demás variantes
de globales. Los ítems T08-004, T08-009, T08-014 y T08-015 siguen abiertos.

## Validación agrupada de sombra y calificadores enum: 2026-11-05

La primera pasada tras `b0c8b304d`, `c53af4477` y `1a941c082` falló porque
una demanda sintáctica de `hidden_global`, que después se resolvía a un local
o parámetro NativeSharp, producía prematuramente un conflicto con `HiddenState`.
`fe469455a` difiere ese caso y `85c23f228` prepara el rechazo complementario
del uso real del global conflictivo.

La segunda ejecución de `call nativesharp-interop\validar-tanda-08.cmd`
terminó con código 0. Pasaron ON/OFF, suite Clang, restauración, equivalencia y
los siete proyectos. El proyecto de enum comprobó lectura del puntero global,
lectura del global `const`, ejecución de la sombra local y rechazo del uso real
conflictivo. `collision.log` registró `C/C++ global type is not yet mapped`;
`const-write.log` registró `left side of assignment is not assignable` para
`readonly_mood`.

La selección especulativa todavía no constituye resolución completa de
sombras y colisiones. Estos casos son Windows x64 O0; no demuestran TLS,
`volatile` enum, punteros con calificadores ni otras configuraciones.

## Validación agrupada de globales enum C: 2026-11-05

Con los commits `2549bbd07`, `c8c37c02e`, `3712a45be` y `e11d160db`,
`call nativesharp-interop\validar-tanda-08.cmd` terminó con código 0. Pasaron
las construcciones incrementales ON/OFF, la suite Clang, las pruebas de
restauración y equivalencia de serialización y los siete proyectos preparados.

El proyecto mixto asignó y leyó desde NativeSharp un global C enum cuyo tipo
también aparece en una firma; además leyó otro global cuyo enum no se nombra
en NativeSharp ni en las firmas utilizadas y comparó su valor con una función
C de resultado entero. En ambos casos se emplearon Windows x64 y O0. La
implementación y estas pruebas no demuestran todavía sombras locales, todas
las colisiones, punteros, TLS, calificadores ni todos los valores de enum;
T08-004, T08-009 y T08-018 continúan abiertos.

## Validación agrupada de enums y enteros: 2026-11-05

Tras los commits `93b2d50a6`, `a3ebbf772` y `e7a7dad05`, la ejecución de
`call nativesharp-interop\validar-tanda-08.cmd` terminó con código 0. La
construcción incremental ON y frontend OFF, la suite de sesión Clang, las
pruebas de restauración y equivalencia y los siete proyectos preparados pasaron.

Se ejecutaron desde NativeSharp una llamada a C con aliases de `signed char`,
`unsigned short` y `unsigned int`; un callback invocado desde C con parámetro y
resultado enum; y escrituras a un campo enum y un elemento de arreglo enum de
un struct C, comprobadas por una función C. Son casos Windows x64 O0 con los
valores elegidos. No demuestran todos los anchos, límites, operaciones,
configuraciones ni combinaciones de tipos; T08-006, T08-009, T08-010,
T08-016 y T08-017 siguen abiertos.

## Validación agrupada de callbacks y tipos C: 2026-11-05

Después de los commits `635124b69`, `945b14d26`, `fd48eef76` y
`8e48b2db3`, `call nativesharp-interop\validar-tanda-08.cmd` terminó con
código 0. Pasaron la construcción incremental ON y frontend OFF, la suite de
sesión Clang, las pruebas de restauración y equivalencia de serialización y
los siete proyectos preparados. La construcción notificó advertencias ya
observadas sobre inicializadores de `deferredImportError`.

El proyecto de callback ejecutó una llamada C con `Callback*` y llamadas de
`float` y `double` usando typedefs; la suite verificó semánticamente un
`ColorCallback*` con enum en su firma. El proyecto de tipo incompleto validó
el puntero a una matriz `[][3]` y el rechazo de `sizeof` de esa matriz. Estos
resultados corresponden a Windows x64, O0 y los casos preparados: no prueban
todas las firmas, calificadores, operaciones o configuraciones, y no cierran
T08-007, T08-013 ni T08-017.

## Validación agrupada posterior: 2026-11-05

`call nativesharp-interop\validar-tanda-08.cmd` terminó con código 0 tras
corregir la referencia recursiva de la prueba de serialización: le faltaba
`enclosingTypeParameter`, campo que ya escribían y leían los codecs de
producción. Pasaron las cuatro comprobaciones de equivalencia, las seis de
restauración de sintaxis, la suite de sesión Clang y los siete proyectos
preparados. El detalle de la corrección consta en
[`t08-referencia-serializacion-constraint.md`](registro-commits/t08-referencia-serializacion-constraint.md).

En esa ejecución se verificaron los casos preparados de arreglo incompleto,
global C volatile y round trip de restrict. Las notas posteriores que los
describen como pendientes de ejecución conservan su estado histórico. La
pasada no demuestra todas las formas de arreglos, calificadores ni el resto de
los contratos de T08-002 a T08-020: esas casillas permanecen abiertas.

## Primera validación agrupada: fallida

La construcción ON y el frontend OFF terminaron tras corregir una llamada a
una API inexistente de Clang. Pasaron los proyectos de valores, tipo incompleto
y funciones enum. La suite informó fallos de refinamiento estructural, layout
de enum y dos casos de tipo incompleto. Los proyectos de union y layout
rechazaron campos importados; callback falló en el propietario de lambda; el
caso record-c se detuvo por una opción inválida del volcado LLVM. Se corrigió
esa opción y se preparó una corrección que transporta dimensiones de campos
desde Clang. Ninguna corrección posterior se ha vuelto a validar. Siguiente:
diagnosticar en conjunto los fallos restantes y repetir la batería agrupada.

## Tercera validación agrupada: casos preparados aprobados

`validar-tanda-08.cmd` terminó con código 0. La construcción incremental de
`nsc` y de `nsc_cpp_session_tests` con interoperabilidad activada, y de
`nsc_frontend` con interoperabilidad desactivada, terminó correctamente. La
comprobación del runtime estático del compilador pasó. La suite de sesiones
Clang y los siete proyectos preparados (valores, incompleto, record C, union,
layout, callback y enum en función) terminaron con código 0.

El proyecto record C comprobó acceso a campos anidados y matriz, un global
compartido con C y una llamada con punteros restrict a objetos distintos. Su
LLVM contiene `store volatile` y `load volatile` para el campo probado. Los
proyectos se construyeron a O0 para Windows x64, con Clang 23.1.0; los casos
aprobados no demuestran todos los layouts, calificadores, callbacks, ABI de
agregados ni configuraciones de optimización. Persisten advertencias de
inicialización en la construcción. Las observaciones detalladas posteriores
de este documento que digan «sin validar» deben interpretarse a la luz de
esta pasada solo para los casos concretos mencionados aquí.

Siguiente acción: contrastar T08-001 a T08-020 con evidencia y código, cerrar
solo los puntos cuya formulación completa esté cubierta y resolver los huecos
sin presentar diagnósticos o una muestra limitada como interoperabilidad total.

### Seguimiento posterior a la tercera pasada

T08-001 queda cerrado como auditoría. La expansión de un arreglo incompleto
todavía se rechaza aunque su snapshot conserve el límite desconocido, por lo
que esa observación no basta para cerrar T08-013. Se preparó un nuevo caso de
global C volatile con verificaciones LLVM diferenciadas; esa ampliación aún no
se ha ejecutado. `TypeModifierSyntax::importedCRestrict` tampoco aparece en el
codec de caché de sintaxis (`TemplateCatalogCodec.cpp`): hay que determinar y
verificar su conservación o invalidación en esas rutas antes de cerrar
T08-015. Las pruebas aprobadas de la tercera pasada no cubren ese caso.

Se preparó una representación diferenciada de arreglos C incompletos y la
expansión inicial de sus typedefs, con casos para el tamaño del puntero y el
diagnóstico al pedir el tamaño del arreglo. También se preparó conservar
`restrict` en el codec de sintaxis y se versionaron las cachés afectadas. Estas
modificaciones y el caso nuevo de global volatile requieren una nueva
construcción y validación agrupada; no cierran todavía T08-013 ni T08-015.

- `ClangSession.cpp::describeType` conserva clase, spelling, tipo canónico,
  calificadores y tamaño/alineación cuando Clang dispone de ellos. Modela
  punteros, referencias, arreglos constantes, enums, records y funciones, pero
  no toda esa información se proyecta todavía a NativeSharp.
- `CppScalarTypeMapping.cpp` mapea enteros por ancho y signo, `bool`, `float`,
  `double` y `void`; `CppSemanticFunction.cpp` añade determinados punteros y
  conserva el símbolo y la identidad canónica. Esto no certifica los ABI de
  agregados ni todos los calificadores.
- La captura de records distingue struct, class y union y obtiene campos,
  offsets, tamaño y alineación de Clang. El proveedor semántico de campos
  excluye union; existen barreras para tipos importados por valor. La captura de
  un record no prueba que sea utilizable en código NativeSharp.
- No se encontró un inventario de declaraciones `typedef` ni de valores de
  enumeradores en `src/Interop/Cpp` y sus interfaces. `CppType` identifica
  `Enumeration` y `Function`, pero no representa por sí solo los enumeradores ni
  la firma completa de un puntero a función.
- `ProjectNameLookup` expande aliases NativeSharp mediante `TypeSyntax`; se debe
  decidir cómo conectar aliases extranjeros conservando identidad, ámbitos,
  colisiones y las reglas de acceso, en vez de fabricar equivalencias.

Siguiente acción: completar el diseño de captura y proyección de aliases, enums,
struct/union y punteros a función; implementar las partes y pruebas de la tanda
como conjunto y ejecutar su validación incremental agrupada. Las limitaciones de
caché y del servicio de lenguaje de tandas anteriores siguen vigentes.

## Avance de implementación sin validar

Los cambios posteriores a la auditoría añaden snapshots de `typedef` y enums,
la descripción estructurada de firmas de función y la conservación del elemento
de arreglos incompletos. Las pruebas correspondientes están preparadas, pero
todavía no se han construido ni ejecutado en la tanda 08. Ninguna casilla de
importación se cierra por estas capturas.

El parser recibe sus nombres externos mediante `prepareCppScopedParserNames` y
`cppParserProjectInventories` antes del análisis sintáctico. La expansión de
aliases en `SyntaxBinding.cpp` usa `LookupEnvironment::typeAliases` y objetos
`TypeSyntax` con vida suficiente; actualmente los snapshots de Clang no llegan a
ninguno de esos puntos. El siguiente trabajo es conectar ambas fases respetando
visibilidad entre proyectos, colisiones y calificadores, y después continuar con
enums y registros. No se considera terminado un alias por reconocer su nombre.

### Procedencia de alias y enums

`CppSemanticInventory.h` y `CppSemanticInventory.cpp` contienen ahora
inventarios que asocian cada observación de alias o enum al ordinal del proyecto,
la referencia y el triple. No fusionan declaraciones homónimas. Las pruebas
preparadas abarcan referencias distintas, valores diferentes y una sesión cerrada;
siguen sin ejecutar hasta la validación agrupada. Los inventarios todavía no se
conectan al parser ni a `ProjectNameLookup` y no deciden compatibilidad entre
declaraciones. El próximo paso sigue siendo esa conexión, no marcar T08-005 ni
T08-009 como terminados.

### Conexión inicial de typedef, sin validar

El proveedor opcional conserva alias durante `ProjectCompilation`; los nombres
se incorporan al inventario sintáctico por proyecto y las observaciones llegan a
`ProjectNameLookup`. Se agregó una expansión inicial para escalares y punteros
representables, y una prueba integrada preparada con `Count`. El driver aún no
instala el proveedor. Faltan tipos compuestos, reglas completas de colisión y
calificadores, activación del flujo normal y la validación agrupada; ningún ítem
se marca cerrado por este avance. Continuar desde esas conexiones pendientes.

### Avance adicional, sin validar

Se preparó la expansión de aliases a records importados y a arreglos fijos de
escalares, con pruebas de identidad y dimensiones. En los snapshots de records
se distingue C de C++; sus campos C se consideran accesibles y la proyección
inicial admite campos escalares de `union` C, conservando la barrera para C++.
También se captura el tipo de expresión de cada enumerador por separado del
tipo de almacenamiento del enum. Ninguno de estos cambios ha pasado la batería
de la tanda: faltan, entre otros, la integración completa de enums y constantes,
casos compuestos, activación del proveedor y pruebas de ejecución de unions.

### Enumeradores C, sin validar

Se preparó la conversión de valores C con verificación de ancho y signo, un
estado semántico que distingue enumeradores de variables externas, el acceso al
valor desde el evaluador entero y un proveedor que reúne los nombres solicitados.
La combinación con variables conserva una colisión como error diferido. Hay
pruebas preparadas de conversión, conflicto y uso en `switch`; el driver todavía
no activa la ruta. Faltan la representación del tipo enum, casos C++ y pruebas
ejecutadas de constantes y expresiones. T08-009 sigue sin cerrar.

### Tipos enum y agregados anónimos, sin validar

Se prepararon tipos enum C con su entero subyacente, una sintaxis sintética
para que el modelo existente consulte ese entero, y la expansión de typedef a
un enum importado. Las pruebas de snapshot, proveedor y enlace semántico
están preparadas. La captura consulta `getCanonicalTagType` de Clang 23.1.0.
La selección de enums aún puede reportar conflictos incluso si no se usan;
es necesario revisar su resolución diferida antes de activar el flujo normal.

La captura de records C conserva el nombre de typedef asociado por Clang a
un struct o union anónimos, en la declaración y en la identidad de tipo. Se
preparó una prueba de ambas formas. No se ha compilado ni ejecutado esta
implementación, ni se ha demostrado la importación de esos tipos por el driver.

El proyecto `pruebas-tanda-08-layout` prepara una comparación de tamaño,
alineación y offset frente a funciones C compiladas por Clang desde el mismo
header. El script está preparado; la comparación aún no se ha ejecutado y no
demuestra cobertura de otros layouts.

`pruebas-tanda-08-union` prepara un recorrido donde NativeSharp escribe un
miembro de una union C y C lee el byte superpuesto. El script existe, pero no
se ha ejecutado y no cubre paso de la union por valor.

Siguiente trabajo: completar la activación coordinada de proveedores de tipos,
aliases y valores para referencias C, resolver conflictos por demanda y las
formas compuestas, incluyendo punteros a función. Después ejecutar la
construcción incremental y validación agrupada de la tanda completa. Ninguna
casilla se cierra por estos cambios.

### Punteros a función: proyección inicial, sin validar

`CppAliasFunctionType.h` proyecta firmas con prototipo explícito y componentes
escalares representables; conserva convención de llamada y variadicidad. La
expansión de typedef construye el tipo estructural de función existente de NSC.
Hay una prueba preparada contra un callback variádico y otro sin prototipo.
Faltan la prueba semántica integrada y la ejecución de llamadas, los tipos
compuestos en la firma, los modificadores del sitio de uso y la activación del
proveedor en el driver. El mapeo no demuestra interoperabilidad ejecutable.

La proyección de firmas de funciones C prepara ahora parámetros que son
punteros a función con componentes escalares. Conserva convención y
variadicidad; rechaza los casos cuyo const del apuntado se perdería. Hay una
prueba de identidad preparada, pero faltan callbacks ejecutables, firmas
compuestas y la validación agrupada.

El proyecto `pruebas-tanda-08-callback` y su script preparan el recorrido de
una lambda sin capturas a través de un typedef y una función C. Todavía no se
ha comprobado que el frontend acepte esa asignación ni que la llamada funcione.

### Conexión inicial al controlador, sin validar

El controlador instala el proveedor de typedef para grafos con sesiones C/C++
y combina los enumeradores solicitados con las variables globales. Se preparó
una prueba de enlace de callback en `ProjectCompilation`. Los proveedores de
tipos enum y records C todavía requieren integración y selección segura; queda
pendiente una prueba del flujo real del controlador. Estos cambios no se han
compilado ni ejecutado y no cierran ningún ítem de la tanda.

### Selección preliminar de records C, sin validar

La observación final de records recibe los typedef de las sesiones para poder
seleccionar un record usado mediante su alias. Se preparó una prueba de ese
recorrido. El controlador configura selección y campos para sesiones C sin C++;
el proyecto `pruebas-tanda-08-valores` cubre la ruta real de typedef escalar y
constantes enum. Falta conectar los tipos enum al selector sin que conflictos
no usados bloqueen proyectos y verificar interacciones con funciones, records,
aliases y la caché. No se ha ejecutado ninguna de estas pruebas.

El proveedor de funciones también recibe el índice de records en sesiones
exclusivamente C. Se preparó `pruebas-tanda-08-record-c`: una biblioteca C
entrega un puntero a struct y NativeSharp modifica un campo antes de consultar
el valor desde C. La prueba aún no se ha construido ni ejecutado. Siguiente
acción: preparar su ejecución reproducible, integrar selección de enums por
uso y completar los casos compuestos antes de la validación agrupada.

### Conflictos diferidos de enums C, sin validar

Las definiciones incompatibles de un enum se conservan en el candidato con un
error diferido y el enlace de nombres de tipo informa ese error al usarse. Se
preparó una aserción de recolección para dos definiciones distintas. Quedan por
comprobar el diagnóstico integrado en usos de tipo y expresión, la selección
junto a records, las colisiones con tipos NS y el flujo del driver. El script
de `pruebas-tanda-08-record-c` quedó preparado, sin ejecutar.

### Selección conjunta de enums y records, sin validar

El controlador suministra candidatos de ambos tipos. La observación resuelve
enums junto a records, conserva los enums usados y el selector los instala con
los records seleccionados. Se preparó una prueba que usa ambos vía typedef.
Todavía deben comprobarse colisiones con declaraciones NS, candidatos de
expresión, firmas de funciones y resultados de ejecución. La implementación y
las pruebas de esta sección no se han compilado ni ejecutado.

### Colisión de enum con otro tipo, sin validar

La observación registra los enums que comparten identidad con un tipo NS o un
record y devuelve un error si un enlace de tipo o candidato de expresión los
utiliza. Se prepararon casos de colisión usada y no usada. Falta verificar
aliases, ámbitos, diagnósticos de uso real y la ejecución agrupada; la prueba
todavía no se ha ejecutado.

### Records incompletos y comparación de miembros, sin validar

El candidato importado distingue declaraciones sin definición de layouts aún
no admitidos; el diagnóstico de layout explica la incompletitud. Se preparó una
aserción de inventario. También se amplió la comparación de miembros repetidos
para conservar firmas de función e identidad y base de enum. Falta probar el
diagnóstico desde un proyecto y ejecutar la batería agrupada.

Los campos C de arreglos fijos escalares cuentan con una proyección inicial y
una prueba preparada de matriz multidimensional y arreglo flexible. La
indexación, los elementos compuestos y el ABI aún no se han validado.

La proyección inicial de campos C admite un record importado anidado por valor
si el propietario, tamaño y alineación coinciden; hay una prueba preparada de
identidad y offset. No habilita paso general por valor y el acceso y la copia
todavía no se han comprobado en ejecución.

También se preparó la proyección de arreglos fijos de records C completos, con
una prueba de identidad, dimensiones y offset. Quedan sin verificar la
indexación, copia de elementos y compatibilidad ABI.

`pruebas-tanda-08-incompleto` prepara ambos casos de proyecto: declaración de
puntero al tipo adelantado y rechazo de `sizeof` con diagnóstico. El script no
se ha ejecutado; la comprobación real sigue pendiente.

El análisis semántico prepara ahora un diagnóstico específico para `sizeof` y
`alignof` sobre el valor incompleto. El caso positivo incluye `sizeof` del
puntero para distinguirlo del valor. Ambos siguen sin ejecutarse.

### Enum C anónimo con typedef, sin validar

La captura usa el nombre de typedef asociado por Clang como identidad del enum
anónimo, tanto en la declaración como en el tipo. Hay una prueba de snapshot
preparada y un script para el proyecto de valores C. Permanecen pendientes los
parámetros, resultados y campos de enum, así como la validación agrupada; el
script y la prueba no se han ejecutado.

### Enum C en firmas, sin validar

La proyección de funciones consultando el índice importado prepara parámetros,
resultados y punteros a enum con identidad nominal y verifica el entero base,
tamaño, alineación y conflictos. Se preparó una prueba de parámetro y resultado
con `Color`. La selección transitiva del enum desde una firma, el caso const,
los campos y la ejecución de llamadas siguen pendientes. No se ha compilado ni
ejecutado esta implementación.

El cálculo de layout de campos NativeSharp acepta el layout conocido de un
enum C importado; hay una prueba preparada de un struct que lo contiene. Los
records importados conservan las barreras de valor y ninguna de estas rutas se
ha validado todavía.

Se agregó un inventario que identifica enums presentes en resultados y
parámetros de funciones, con su profundidad e índice de candidato; una prueba
preparada cubre ambos. Falta usar las demandas reales de llamadas para elegir
esas dependencias antes del índice definitivo. Ninguno de los cambios de esta
sección se ha compilado ni ejecutado.

Se preparó además la selección de candidatos enum a partir de demandas de
funciones, incluyendo sobrecargas no proyectadas y conflictos diferidos, con
una prueba de deduplicación. El siguiente paso es obtener las demandas durante
la observación previa y conectarlas al selector definitivo; sigue sin validarse.

El inventario de firmas prepara además el recorrido de resultados y parámetros
de callbacks para identificar enums anidados. Hay una prueba preparada con
dos profundidades de indirección; la proyección y ejecución de callbacks con
enums siguen sin validar.

La proyección de firmas exteriores prepara callbacks con enums y punteros a
records importados mediante resolución recursiva. Hay una prueba de identidad
de enum en el parámetro y resultado del callback. Falta conectar typedefs de
callbacks compuestos y comprobar llamadas reales; nada de esto se ha ejecutado.

La observación previa construye ahora modelos locales para capturar llamadas
cuando existen firmas con enums y selecciona sus dependencias antes del índice
definitivo. Hay una prueba preparada para un resultado enum no nombrado por
NativeSharp. Faltan comprobar regresiones de genéricos, snapshots, colisiones y
la ejecución ABI; los cambios no se han compilado ni ejecutado.

La expansión de un typedef de callback prepara componentes enum o record con
su identidad importada y convención de llamada. Se agregó una prueba semántica
para un enum en el resultado y parámetro del callback; aún no se ha ejecutado.
Falta comprobar que la observación seleccione dependencias de tipos nombradas
únicamente dentro del typedef, además de callbacks anidados y llamadas reales.
El paso de records por valor mantiene pendiente su verificación ABI.

Se preparó la expansión de callbacks anidados y una prueba de su firma, así
como una prueba de selección de enum y record usados solo dentro de un typedef
de callback. También se preparó la expansión de typedefs de arreglos fijos de
enums o records y una prueba de matriz de enum. Ninguno se ha construido ni
ejecutado. Se prepararon además el caso de matriz de record y una prueba de
selección de enum y record referidos únicamente por typedefs de arreglos.
Faltan las pruebas agrupadas de selección, acceso, layout y ejecución.

La captura de `CppType` prepara ahora `restrict` junto con `const` y `volatile`;
la comparación recursiva de miembros lo distingue. Se preparó una prueba de
snapshot de un typedef con puntero restrict. El símbolo semántico de función
importada prepara la conservación de los tipos originales del resultado y
parámetros, con una prueba de parámetro restrict; no emite `noalias` por ello.
La expansión de typedefs de punteros prepara metadatos restrict por nivel,
incluyendo las rutas de arreglos y componentes de callbacks; se prepararon
pruebas de un nivel y niveles anidados. Falta ejecutar las pruebas y determinar
el tratamiento del contrato en las demás rutas antes de cerrar T08-015.

El proyecto `pruebas-tanda-08-enum-funcion` y su script preparan una prueba
de extremo a extremo: un valor enum negativo se devuelve desde C y se vuelve
a pasar a otra función C desde NativeSharp. Debe devolver cero; aún no se ha
construido ni ejecutado.

### Campos enum C, sin validar

Un helper común comprueba propietario, representación y conflictos del enum.
La proyección de campos de records C lo utiliza para enum y puntero a enum;
una prueba preparada comprueba también el const del apuntado. Sigue pendiente
seleccionar transitivamente enums referidos solo por campos o firmas, probar
acceso y llamadas ejecutables y ejecutar la validación agrupada.

La proyección preparada también acepta campos con arreglos fijos de enum y
comprueba el tipo importado. Hay una aserción para dimensiones y offset; la
indexación y ejecución todavía no se han validado.

El selector incorpora ahora enums referidos por campos de records C
seleccionados, incluso tras punteros o arreglos, y se preparó una prueba donde
solo se escribe el tipo del record. Todavía falta comprobar colisiones con
tipos NS y seleccionar dependencias de firmas; no hay resultados de ejecución.

La observación conserva ahora las identidades de enums que colisionan y el
selector las comprueba también cuando un campo introduce la dependencia. Hay
una prueba preparada con un struct NS homónimo. Las demandas de funciones se
capturan después del índice de nombres (`ProjectCompilationSnapshots.cpp`),
por lo que seleccionar los enums de una firma requiere resolver ese orden;
no basta con proyectar la función al final. Esta ruta no se ha validado.
# Retorno con apuntado volatile declarado

`validatePointerReturn` rechazaba todo retorno de puntero cuyo apuntado era
volatile, incluso si la declaración de destino conservaba el mismo calificador.
Se permite ese caso y se mantiene el rechazo cuando el destino carece de él.
La prueba funcional y la activación del tipo por alias siguen pendientes y se
comprobarán en la validación agrupada de la tanda 08.

## Caso de alias C de puntero a volatile

El proyecto de records declara `VolatileIntPtr` en C y lo usa en un parámetro,
un retorno y un local NativeSharp; escribe a través del puntero y compara con
la función C que lee esa memoria. Es una prueba preparada: la proyección del
alias y la ejecución agrupada aún están pendientes.

## Proyección de alias C a puntero escalar volatile

La resolución de nombres ya prepara la proyección de aliases a punteros de
un nivel con apuntado escalar volatile y conserva const. El mapeo consulta una
copia sin volatile, conserva el original de Clang y registra el calificador en
TypeSyntax. Modificadores externos y formas no representadas siguen rechazados.
La prueba del proyecto mixto y las regresiones de la tanda están pendientes.

## Apuntado volatile en estados globales NativeSharp

El análisis de globales consultaba únicamente el dato de declaraciones C.
Ahora consulta también la sintaxis declarada en NativeSharp y evita formar una
dirección que pierda el calificador en un nivel anidado. La comprobación de
aliases y de los inicializadores globales sigue pendiente de la tanda agrupada.

## Conversión explícita con apuntado volatile declarado

El análisis de cast descartaba el dato volatile del tipo de destino y
rechazaba incluso conversiones que lo conservan. Se transfiere al resultado y
se mantiene el rechazo cuando el destino no lo representa. La comprobación
funcional corresponde a la validación agrupada, aún pendiente.
