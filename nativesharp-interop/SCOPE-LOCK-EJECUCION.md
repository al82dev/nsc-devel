# Alcance de ejecución: interoperabilidad C/C++ desde NativeSharp

## Referencia Clang para arreglos globales C volatile escalares

T08-006/T08-007/T08-015/T08-016: obtener IR de lectura y escritura de
arreglos globales C volatile de `_Bool`, enteros de 8, 16 y 64 bits con y
sin signo, `float` y `double`. Comparar anchos, órdenes y alineaciones con
Clang Windows x64 antes de extender la corrección actualmente limitada a int.

Presupuesto: `nativesharp-interop/pruebas-tanda-08-record-c/Api.h`, `Api.c`,
tracking y registros. IR temporal exclusivamente en `A:/nsc-interop-build`.
No modificar producción basándose solo en esta preparación; conservar NSLink,
LLVM externo y la instalación de C:.

## Índice de puntero obtenido de arreglo global C

T08-015: completar la combinación `arreglo[i][j]` cuando el primer índice
obtiene directamente un `volatile int*` de un arreglo global C y el segundo
índice accede al entero. Conservar el modo MS solamente si la cadena HIR de
índices llega al global C resuelto; no alterar el slot del puntero ni admitir
orígenes locales, campos, parámetros o casts sin su propia procedencia.

Presupuesto: `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp`,
el fixture `pruebas-tanda-08-record-c`, tracking y registros. Comprobar
lectura, escritura e IR; la validación se agrupa con la tanda.

## Orden en el índice directo de global C volatile int*

T08-015: complementar `*global` con `global[indice]` cuando el receptor es
directamente un global C importado `volatile int*` con modo MS. El stride de
int preserva la alineación natural de cuatro; emitir acquire/release sin
transferir la marca al almacenamiento del puntero. Punteros desde locales,
parámetros, campos o resultados de otras expresiones siguen pendientes.

Presupuesto: `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp`,
`src/LLVM/Lowering/PackedArrayAlignment.h`, el fixture
`pruebas-tanda-08-record-c`, `tests/unit/CppClangSessionTests.cpp`, tracking y registros. Conservar las rutas de
arreglos y campos existentes; validar la escritura y la lectura indexadas con
IR y ejecución junto a los demás cambios de la tanda.

## Orden al desreferenciar puntero obtenido directamente de arreglo global C

T08-015: el fixture de arreglos de `volatile int*` ejecuta correctamente,
pero su IR muestra accesos volatile sin acquire/release, mientras Clang MS
emite esos órdenes para el apuntado int. Extender la ruta de desreferencia
directa únicamente cuando la carga del puntero procede de elementos de un
arreglo global C importado, con modo MS conocido y el calificador conservado.
No aplicar la marca al almacenamiento del puntero, ni a locales, parámetros,
campos o casts; esos contratos siguen pendientes.

Presupuesto: `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp`,
el fixture `nativesharp-interop/pruebas-tanda-08-record-c`, tracking y
registros. Verificar origen siguiendo exclusivamente nodos de índice fijo
hasta el global C resuelto; incluir matriz, `const volatile`, puntero constante
y controles IR. No modificar NSLink, LLVM externo ni la instalación de C:.

## Orden al desreferenciar directamente un global C volatile int*

T08-015: el IR comparado muestra que Clang MS usa acquire/release al acceder
al entero mediante un `volatile int*`, mientras el IR NS anterior omite el
orden. Tratar exclusivamente `*global` cuando el operando resuelto es el
valor directo de un global C importado `volatile int*` con modo MS conocido.
El puntero C válido a int debe satisfacer la alineación natural de cuatro;
no aplicar esta inferencia a casts, miembros, parámetros, locales, arreglos de
punteros ni a otros tipos. Esas rutas siguen pendientes por procedencia.

Presupuesto: `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp`,
`src/LLVM/Lowering/PackedArrayAlignment.h`,
`src/LLVM/Lowering/OopLlvmArraysAndStores.cpp`,
`src/LLVM/Lowering/OopLlvmExpressionMachine.cpp`,
`tests/unit/CppClangSessionTests.cpp`, el fixture
`pruebas-tanda-08-record-c`, tracking y registros. Los controles deberán
comparar lectura y escritura de ese global; no se modifican NSLink ni LLVM.

## Orden de accesos a arreglos globales C volatile int

T08-015: Clang Windows x64 emitió `load atomic volatile i32 ... acquire` y
`store atomic volatile i32 ... release` para `volatile_array[1]` y
`volatile_matrix[1][1]`; un IR NS previo emite accesos volatile sin esos
órdenes. Corregir únicamente elementos `int` de arreglos globales C importados
con modo MS y alineación de al menos cuatro bytes acreditada por Clang.
Conservar accesos ordinarios ante alineación insuficiente o modo desconocido;
no modificar campos, punteros arbitrarios, otros tipos ni NSLink.

Presupuesto: `src/Frontend/HIR/ExpressionStatementMachine/HirNameExpressionLowering.cpp`,
`include/nativesharp/Frontend/NamespaceStateSymbol.h`,
`src/Interop/Cpp/CppSemanticInventory.cpp`,
`src/LLVM/Lowering/PackedArrayAlignment.h`,
`src/LLVM/Lowering/OopLlvmArraysAndStores.cpp`,
`src/LLVM/Lowering/OopLlvmExpressionMachine.cpp`,
`tests/unit/CppClangSessionTests.cpp`, el fixture
`pruebas-tanda-08-record-c`, tracking y registros. Demostrar la procedencia
desde un global externo, la alineación del origen y la de cada paso antes de
asignar órdenes; validar el IR y ejecutar la batería agrupada incremental.

## Punteros globales C a volatile void

T08-004/T08-008/T08-015: el mapeador de funciones C admite un parámetro
`volatile void*`, pero `CppSemanticInventory.cpp` rechaza explícitamente
ese mismo tipo como global. Permitir su proyección inmediata con el mismo
calificador, sin habilitar apuntados anidados ni alterar C++. El objeto void
no se desreferencia: se pasa a una función C declarada con su calificador.

Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`,
`tests/unit/CppClangSessionTests.cpp`, el fixture
`nativesharp-interop/pruebas-tanda-08-record-c`, tracking y registros. Añadir
controles de global y arreglo, paso a función, rechazo de descarte de volatile
y rechazo de desreferencia; validar junto a la tanda con construcción
incremental. LLVM, NSLink y la instalación de C: permanecen protegidos.

## Arreglo global C de punteros a volatile

Objetivo T08-015/T08-016: conservar el calificador del objeto apuntado en un
arreglo global C de punteros a `volatile`, siguiendo el camino existente para
punteros a `const`. El slot del arreglo permanece asignable; los accesos al
objeto apuntado deben conservar volatile y las conversiones que lo descartan
deben rechazarse. La representación actual rechaza expresamente estos arreglos
en `CppSemanticInventory.cpp`; el análisis de índices solo propaga el
calificador de apuntado readonly.

Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`,
`src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp`, los fixtures
de `nativesharp-interop/pruebas-tanda-08-record-c`,
`tests/unit/CppClangSessionTests.cpp`, tracking y registros de
esta tanda. Preservar punteros anidados que no puedan representarse, NSLink,
LLVM y la instalación en C:. Añadir controles de lectura, escritura,
conversión que descarte volatile y dirección del slot; comprobar IR y
ejecución en validación agrupada incremental. No declarar completado el ítem
por esta única forma de declaración.

## TLS de una DLL C

Preparar un fixture separado `pruebas-tanda-08-tls-c/directo` con un global
TLS exportado por DLL, lectura y escritura directa desde NS y control de
aislamiento desde C. Generar además un consumidor C con Clang/lld y registrar
sus relocaciones como referencia del ABI. Usar solo
`A:/nsc-interop-build/tanda08-tls-directo`; no cambiar producción ni NSLink
hasta observar y delimitar los resultados.
Clang rechazó `thread` junto con `dllimport` y `dllexport` en Windows MSVC.
Probar en el mismo fixture declaraciones TLS sin esos atributos y exportación
por `/export:...,DATA`; conservar los diagnósticos originales y no asumir que
la biblioteca de importación pueda satisfacer relocaciones TLS de COFF.

El fixture TLS estático falla por `_tls_index` en NSLink. Sin modificarlo,
preparar una variante en `pruebas-tanda-08-tls-c` donde Clang y lld construyan
una DLL C con directorio TLS y NSC invoque su función de aislamiento entre
hilos. Usar solo `A:/nsc-interop-build/tanda08-tls-dll` para salidas y las
bibliotecas de `A:/vc2026-runtime` en lectura. Inspeccionar el directorio TLS
y probar O0–O3. Esto prueba TLS dentro de la DLL, no importación directa de
un global TLS ni TLS enlazado estáticamente por NSLink.

## Typedef C en proyectos reales

La comparación mixta devolvió 7: HIR promueve `uint` e `int` a `long`, mientras
Clang C aplica unsigned. Separar ese caso en un proyecto independiente y dejar
pendiente la decisión de semántica solicitada al usuario. No cambiar reglas
numéricas ni hacer pasar la prueba alterando silenciosamente su expectativa.
Separar la suma mixta en un segundo proyecto para obtener su resultado aun si
la comparación se detiene primero. Mantener ambas expectativas C como evidencia,
sin confundir la divergencia con un resultado certificado.

Contrastar operaciones mixtas entre enum C unsigned e int signed mediante
funciones de referencia compiladas por Clang; cubrir comparación y suma.
Registrar cualquier diferencia antes de decidir la semántica o modificar el
compilador, sin cambiar reglas numéricas generales de NativeSharp.

La prueba de enum de 8 bits generó negación i8 y devolvió 5: la ruta iterativa
`src/Frontend/HIR/ExpressionStatementMachine/HirCompletionSimpleFrames.cpp`
omitió la promoción preparada para la ruta recursiva. Permitir ese archivo y
aplicar la misma conversión antes de la operación; comprobar el IR y O0–O3.

Ampliar el grupo de operaciones de enum C con desplazamiento, operador
unario y máscara; comprobar resultado unsigned y mantener control de enum
NativeSharp. Observar fallos antes de modificar esas rutas semánticas.
Comprobar positivamente las operaciones bit a bit del mismo enum NativeSharp
además del control negativo de suma, para conservar su contrato previo.
El fixture rechazó los tres operadores. Permitir cambios en
`src/Frontend/Bodies/OperatorExpressionAnalyzer.cpp`,
`src/Frontend/Bodies/IdentifierAnalyzer.cpp` y
`src/Frontend/HIR/HirExpressionMachine.cpp`, usando promociones del tipo C
importado y sin modificar las reglas de enum NS. Cubrir también enum de 8 bits
para verificar que se promueva antes de la operación LLVM.

La prueba de aritmética rechazó sumas con enum C signed y unsigned; el control
de enum NativeSharp también se rechaza. Permitir el ajuste localizado de
`src/Frontend/Bodies/OperatorExpressionAnalyzer.cpp` para usar el tipo
subyacente solo de enums C importados, preservando las reglas de enum NS.
Comprobar lowering, signo y resultado frente a C.
Ampliar el mismo fixture con división y residuo unsigned, comparados con las
funciones equivalentes compiladas por Clang; no inferir esas operaciones de la
sola suma.

Preparar comprobación de operaciones aritméticas sobre enums C importados,
comparando el resultado unsigned con una función C y manteniendo un control
para enum NativeSharp. Registrar fallos antes de modificar producción.

Clang Windows confirma con `_Static_assert` que el enum previo sin tipo fijo
es signed pese al literal unsigned. Fijar explícitamente `unsigned int` en el
fixture (extensión admitida por Clang en C11) y conservar una comprobación C;
no asumir el signo a partir del literal. Repetir la validación.

Preparar pruebas de conversión desde enum C importado a entero y a otro enum
C, incluida extensión de un valor sin signo y control de enum NativeSharp.
Observar errores y delimitar la solución antes de modificar producción.
La prueba rechazó enum C a int, long y otro enum C; el enum NativeSharp también
se rechazó en el control. Permitir cambio localizado en
`src/Frontend/ProjectConversionClassifier.cpp` para conversiones numéricas de
enum C importado a entero o enum C, sin habilitar conversiones de enums NS.
LLVM debe respetar ancho y signo del origen; comprobarlo en O0–O3.
El programa compiló pero terminó con código 3 al ampliar `CUnsigned` a long.
`findEnumUnderlyingInfo` usa el syntax del enum sintético y obtiene int en vez
del tipo `enumBase` importado. Permitir cambio localizado en
`src/LLVM/LlvmTypeLayout.cpp` para consultar el tipo informado por Clang en
enums C, manteniendo la ruta existente para enums NS y probando signo y ancho.

Preparar caso independiente de asignación e inicialización de enumeradores C
a variables, elementos de arreglo y parámetros del enum importado. Comparar
también el tipo de un enumerador en contexto entero y preservar un control de
enum NativeSharp. Registrar fallos y no modificar producción sin delimitar
clasificación y lowering de la conversión.
La ejecución encontró rechazos `builtin:int` a enum C en los tres contextos.
La clasificación central no distingue el origen C; LLVM ya obtiene el ancho y
signo subyacentes para conversiones de enums. Permitir cambios localizados en
`include/nativesharp/Frontend/ProjectInheritanceModel.h` y
`src/Frontend/ProjectConversionClassifier.cpp` para reconocer conversión de
enteros hacia enum C importado, manteniendo intactos enums NativeSharp. Agregar
controles de argumentos, valores variables, ancho y signo, y enum NativeSharp.

Ampliar el fixture con punteros a arreglos fijos de struct y enum C. Probar
identidad, lectura y mutación en C, ejecución O0–O3 y rechazo de const o tipos
incompatibles. No cambiar producción hasta observar los fallos concretos.
Ambas funciones con punteros a arreglos de struct y enum no se importan porque
la proyección rechaza esos elementos. Permitir cambio localizado en
`src/Interop/Cpp/CppSemanticFunction.cpp` para mapear su identidad solo con
tipo importado y layout comprobados; preservar const y seguir rechazando
volatile y elementos puntero. El enum tiene además un fallo independiente de
conversión del enumerador; no usarlo para ocultar el resultado de la llamada.
Inicializar el arreglo enum mediante una función C para aislar esa limitación;
agregar rechazos de punteros const e identidades de elemento incompatibles.

Agregar al fixture una función C que modifica una matriz por puntero y otra
que devuelve puntero a matriz const. Verificar mutación real O0–O3; rechazar
pasar la matriz const o una matriz de dimensiones distintas al parámetro mutable.

Crear `nativesharp-interop/pruebas-tanda-08-typedef-c` para comprobar juntos
typedefs C escalares, cadena de alias, arreglos de dimensiones fijas, enum,
struct y callback en un proyecto NativeSharp enlazado con una biblioteca C.
Probar O0–O3 y registrar las barreras sin modificar producción hasta
identificar la causa. Salidas únicamente en `A:/nsc-interop-build/tanda08-typedef-c`.
La primera ejecución no importó `c_matrix_sum` con parámetro puntero a matriz;
conservar ese hallazgo y cambiar el fixture principal para pasar dos elementos
escalares, de forma que se puedan observar también los demás alias. El
puntero a matriz permanece como contrato pendiente, no se considera resuelto.
Conservar la firma y el consumidor de puntero a matriz en archivos propios del
fixture para poder repetirlos independientemente; no sustituir la comprobación
por un rechazo esperado. Registrar el resultado antes de modificar producción.
La inspección de `mapFunctionType` confirma que no representa el array que
apunta el parámetro. Permitir cambios localizados en
`src/Interop/Cpp/CppSemanticFunction.cpp` para arrays C fijos de escalares
apuntados, reteniendo dimensiones y const; rechazar volatile y formas anidadas
sin representación. Comprobar llamadas y rechazos antes de ampliar el alcance.

## Arreglos globales C de punteros constantes

Extender `pruebas-tanda-08-global-puntero-const` con un arreglo global C
`int * const[2]`: sus slots no pueden reasignarse, pero sus datos apuntados
son mutables. Comprobar lectura, escritura al dato y verificación desde C en
O0–O3; rechazar reasignación del slot y escape por dirección que permita
reasignarlo. Usar salidas en A: y no cambiar producción antes de observar y
delimitar el fallo. Mantener las pruebas previas de punteros a const.
El fixture produjo `C/C++ global type is not yet mapped` para el arreglo de
punteros constantes. Permitir un cambio localizado en
`src/Interop/Cpp/CppSemanticInventory.cpp` para transportar el const del slot
como readonly del arreglo, sin alterar el calificador del dato apuntado y sin
habilitar volatile ni calificadores anidados no representados. Comprobar las
operaciones y escapes antes de declarar soportada esta forma.
Agregar un control que intente pasar el arreglo de slots constantes a una
función C que recibe `int**`; debe rechazarse, ya que podría reasignarlos.
Ampliar el mismo fixture con `int * const[2][2]` y `const int * const[2]`;
probar lectura, modificación permitida de datos de la matriz, y rechazo de
escritura a slots, datos constantes, dirección y paso de filas a `int**`.
No modificar producción hasta observar los fallos y delimitar el cambio.
Agregar controles de escape por dirección del elemento de matriz y del slot
con doble const, con diagnósticos específicos y sin aceptar errores ajenos.

## Pruebas independientes de metadata histórica C

La ampliación a 1.45 descubrió que falta la referencia histórica 1.44. El
archivo `A:/nsc-interop-build/tanda08-flotante-c/CLongDoubleProducer.nsmeta`
fue generado antes de la ampliación y el compilador experimental lo verifica
como NSMETA 1.44 con Object ABI 3. Incorporarlo sin modificar sus bytes como
`tests/fixtures/nsmeta/historical-1.44.nsmeta`, anotando origen y huella.
Permitir cambios localizados en `tests/unit/Stage48_2LibraryMetadataTests.cpp`:
validar esa referencia con su Object ABI real; comparar el formato informado
con el de los bytes emitidos; y probar la etiqueta futura sobre un modelo
serializado con el formato actual, no sobre un payload 1.40 truncado para 1.46.
Las referencias ausentes 1.40–1.43 siguen pendientes; no fabricar ni reescribir
referencias históricas para hacer pasar la prueba.
Permitir `tests/unit/LibraryMetadataVersionToleranceTests.cpp`: distinguir que
el lector puede descodificar la referencia auténtica 1.37 de que el compilador
pueda usarla; exigir el rechazo explícito por Object ABI 2 frente a 3. Mantener
los controles de aceptación de formatos futuros cuyo contenido sea compatible.
Se localizaron productores reales anteriores con metadata 1.40, 1.41, 1.42
y 1.43 que el verificador acepta con Object ABI 3. Conservar sus bytes en
`tests/fixtures/nsmeta/historical-1.40.nsmeta` a `historical-1.43.nsmeta`,
registrar productor y SHA-256, y ajustar la prueba histórica para exigir su
aceptación con ABI 3. No reescribir las referencias archivadas existentes.

## Punteros C a tipo incompleto entre módulos

Ampliar el mismo header de prueba con redeclaraciones adelantadas adicionales
de struct y union opacos, una forma válida de C. Comprobar que el productor y
consumidor que incluyen ese mismo archivo reconcilien su identidad y que se
mantengan los rechazos por otro origen y por modificación del header. No
modificar producción antes de identificar el fallo concreto.

Ampliar `nativesharp-interop/pruebas-tanda-08-incompleto` con una biblioteca C
que define privadamente un struct opaco. Un proyecto NativeSharp consumirá
punteros devueltos por C, los pasará a funciones C y accederá a un global de
puntero al mismo tipo; la prueba negativa verificará que no se invente tamaño
ni se permita un objeto por valor. Mantener salidas en A:, usar Clang y NSC
experimentales existentes y no modificar producción antes de reproducir y
delimitar una falla. Validar la matriz O0–O3 agrupada; LLVM y NSLink son de
solo lectura.
El fixture por valor produce un mensaje genérico de lifetime aunque la
declaración C es incompleta. Permitir una comprobación localizada en
`src/Frontend/Bodies/StatementAnalyzer.cpp` para emitir `IncompleteType` con
el motivo real antes de la barrera de lifetime, únicamente para structs y
unions C incompletos. Preservar el rechazo anterior de tipos completos y
verificar el diagnóstico concreto y las regresiones existentes.
Extender el mismo fixture con un global C `Opaque**` y una `union` opaca,
incluidos retorno, argumento, global y rechazo de local por valor. La
definición de la union permanece privada en el archivo C. Registrar resultados
por separado; no modificar producción antes de demostrar una falla.
Ampliar el fixture con un productor NativeSharp precompilado que publique
funciones con punteros a ambos tipos opacos y un consumidor que use esa
superficie sin importar directamente el encabezado C. Probar O0–O3; no
modificar producción antes de reproducir y delimitar una falla.
Extender el fixture de metadata opaca con un consumidor que importa el mismo
header que el productor y cruza punteros de struct y union entre funciones
C y métodos publicados. Comprobar O0–O3; no modificar producción antes de
reproducir y delimitar una falla.
El consumidor con el mismo header se detuvo por conflicto de alias `Opaque`.
La captura `ForwardOnly` descarta procedencia, el codec no admite procedencia
en incompletos y la comparación los considera insuficientes. Permitir cambios
localizados en `src/Interop/Cpp/CppSemanticRecord.cpp`,
`src/Interop/Internal/CppCRecordMetadataCodec.cpp`,
`src/Interop/LibraryMetadataConstruction.cpp`,
`src/Interop/LibraryMetadataVerification.cpp`,
`include/nativesharp/Interop/LibraryMetadata.h`, la suite de sesión, el fixture
opaco y sus registros. Conservar lectura y bytes de catálogos antiguos;
versionar la extensión y promover metadata únicamente cuando se use. Solo
conciliar declaraciones incompletas con mismo tipo, USR, triple y procedencia
directa verificada por archivo, huella y desplazamiento. Rechazar información
ausente o discrepante; probar también headers modificados o distintos. No
desplegar ni cambiar NSLink o LLVM.
La suite encontró expectativas históricas que ahora reciben procedencia también
para el record incompleto y por eso dejan de representar los catálogos 1/2.
Mantener esos controles con sus datos históricos y agregar controles explícitos
de las nuevas versiones. Permitir `src/Interop/LibraryMetadataPayload.cpp`: la
proyección ABI reconoce el formato flotante solo en catálogo 2 y debe hacerlo
también en 4 al retirar la procedencia, sin cambiar las huellas antiguas.
La primera construcción con procedencia pasó el conflicto de alias, pero el
fixture se detuvo al proyectar `opaque_current` y `union_current`: el global
opaco exige que el propietario importado sea el proyecto directo incluso si
la identidad ya fue reconciliada. Permitir el ajuste localizado en
`src/Interop/Cpp/CppSemanticInventory.cpp` para emplear la identidad canónica
reconciliada, conservando controles de origen, USR y rechazo de conflictos.
Ampliar el fixture con un consumidor que usa una copia del header en otra
ruta y solicita intercambiar los punteros; debe rechazarse sin evidencia de
origen común. Copiar únicamente al directorio de pruebas en A: y conservar el
header original.
En el mismo fixture, después de generar el productor, modificar únicamente la
copia del header situada en el directorio de salida y comprobar que el
consumidor la rechaza por falta de procedencia coincidente. El archivo fuente
original queda intacto y se restaura la copia al iniciar cada ejecución.
La prueba positiva reproduce el rechazo del global `Opaque*` en O0. Las
firmas de funciones aceptan el puntero, pero `mapGlobalType` exige layout al
recorrer su objeto apuntado. Permitir `src/Interop/Cpp/CppSemanticInventory.cpp`
para proyectar ese puntero C con identidad y procedencia comprobadas, sin
permitir un objeto incompleto por valor ni fabricar tamaño. Conservar el resto
de las restricciones y ejecutar el fixture agrupado y la regresión existente.

## Arreglos globales C de punteros a const

Preparar el fixture `nativesharp-interop/pruebas-tanda-08-global-puntero-const`
para un arreglo global C de dos punteros a `const int`: leer el dato, cambiar
un elemento del arreglo por otro puntero compatible y verificar el resultado
desde C; rechazar que se escriba a través del puntero al dato const. Mantener
las salidas bajo `A:/nsc-interop-build`, registrar los resultados y no modificar
producción antes de reproducir el fallo y acotar la ruta responsable. LLVM y
NSLink permanecen de solo lectura.
Ampliar el mismo fixture con controles que rechacen perder el `const` al
asignar un elemento a `int*` y al obtener la dirección del slot, ya que el
tipo anidado tendría que conservar el calificador. No habilitar la proyección
hasta conservar estas protecciones.
El caso positivo falla porque `mapGlobalType` rechaza `pointeeConst`; el
estado global solo transmite ese dato para un puntero superior y
`analyzeRawIndex` no lo transporta al puntero resultante. Permitir cambios en
`src/Interop/Cpp/CppSemanticInventory.cpp` y
`src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp` para el caso
C global unidimensional de punteros simples a escalar const: proyectar el
tipo, conservar el calificador al indexar y no permitir obtener la dirección
del slot como puntero sin calificadores. Mantener otros arreglos calificados
rechazados; validar lectura, reasignación, escritura, escape y dirección con
la batería agrupada y pruebas existentes de arreglos.
Ampliar los controles del mismo fixture con un puntero inferido desde un slot
y con el arreglo utilizado como argumento de una función que acepta `int**`.
Ambas rutas deben rechazar la pérdida de const; no modificar producción hasta
reproducir una falla específica.
Extender el fixture con una matriz C `const int *[2][2]`, lectura, reasignación
y comprobación desde C; preparar rechazos de escritura y conversión a puntero
mutable para un elemento. No modificar producción antes de reproducir el fallo
y acotar la propagación del calificador entre dimensiones.
Ampliar los controles de la matriz con direcciones de elemento y de fila,
y con el paso de una fila a `int**`. Las pruebas deben conservar el contrato
del dato apuntado; no modificar producción sin registrar una falla concreta.
El fixture positivo confirmó `C/C++ global type is not yet mapped` para la
matriz. Permitir `src/Interop/Cpp/CppSemanticInventory.cpp` y
`src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp` para
proyectar arreglos C de dimensiones fijas con elemento puntero simple a
escalar const, conservar la marca entre índices y proteger direcciones que la
perderían. Mantener los demás casos no representables rechazados; comprobar la
matriz, sus controles negativos y la regresión de layout.
El caso positivo y los rechazos pasaron, pero pedir `&c_slots[0]` emitió el
mensaje genérico de campo de bits, aunque el elemento no lo es. Permitir una
comprobación localizada en `IndexAndRangeExpressions.cpp` y ajustar el script
del fixture para exigir mensajes de error reales, sin aceptar coincidencias
accidentales con el nombre de la ruta.
Los controles posteriores de direcciones de matriz muestran el diagnóstico
específico junto con el genérico de bitfield. En la misma comprobación de
`IndexAndRangeExpressions.cpp`, marcar el resultado inválido tras emitir el
error para que el operador `&` no agregue un diagnóstico engañoso. Exigir la
ausencia del mensaje genérico en esos controles y validar junto con la tanda.

## Funciones C static inline de encabezados

Investigar la importación y materialización de funciones C `static inline`
desde encabezados, incluidas funciones que llaman a símbolos externos y
con estado estático. Crear un fixture independiente en
`nativesharp-interop/pruebas-tanda-08-inline-c` y conservar resultados en
`A:/nsc-interop-build`. No modificar producción hasta reproducir el fallo y
registrar los archivos y el diseño de integración; LLVM permanece de lectura.
El fixture rechaza los identificadores `static inline`. Preparar emisión de
IR con CodeGen oficial en una unidad por sesión, solicitando varias funciones
juntas para compartir sus estáticos internos. Permitir `CppInteropSession.h`,
`ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp` y registros. Esta
etapa aislada no expone funciones ni las enlaza; quedan pendientes identidad
única por proyecto, selección de demandas, objeto y enlace antes del fixture.
Permitir ahora `CppFunctionDeclaration.h`, `ClangSession.cpp`,
`CppSemanticInventory.h/.cpp`, `CppSemanticFunction.cpp`,
`CppLifetimeObjectEmitter.h/.cpp`, `DriverArtifactGeneration.cpp` y tests de
sesión y fixture para integrar funciones C static inline seleccionadas. Crear
símbolos estables separados por proyecto y referencia, emitir juntas por sesión
para preservar estado interno y añadir los objetos al bundle del producto.
Verificar funciones dependientes, estado estático, llamadas externas y O0–O3;
probar también bibliotecas precompiladas y colisiones antes de ampliar el cierre.

Ampliar ese fixture con `ProducerSecond.ns`, `ProducerSecond.nsproj` y su
consumidor existente para enlazar dos bibliotecas NativeSharp precompiladas que
incluyen el mismo encabezado C. Comprobar que cada una conserva contador y
estado estático propios, de O0 a O3. Permitir únicamente esos archivos, el
script del fixture y registros; no cambiar producción sin observar y delimitar
un fallo concreto.

En el fixture de dos productores, ambos declaran el mismo `api.lib` y el
consumidor falla con NSL1021 en `external_double`. La construcción estática
con `buildStaticObjects` no incorpora esa biblioteca al archivo productor;
`validateLibraryReference` añade la ruta de metadata para cada productor sin
consultar `linkedLibraries`. Permitir exclusivamente
`src/Driver/LibraryReferenceValidation.cpp`, el proyecto del segundo productor,
el tracking y los registros para omitir una dependencia C/C++ cuya misma ruta
ya fue añadida por un productor anterior. Conservar orden y repeticiones dentro
de cada lista individual, validación de identidad de metadata y NSLink intacto;
probar los dos productores con la misma dependencia y la batería agrupada.
Ampliar el fixture existente para que ambos productores llamen al símbolo C
externo y el consumidor declare y utilice directamente el mismo encabezado y
la misma biblioteca. Se permiten `ProducerSecond.ns`, `Consumer.ns`,
`Consumer.nsproj` y sus registros; no modificar producción sin reproducir y
delimitar una falla.

## Redeclaraciones C con prototipo más preciso

Ampliar `pruebas-tanda-08-prototipos-c` con declaraciones compatibles `f()`
seguidas de `f(int)` y `g(void)`, comprobando llamadas válidas y rechazo de
aridad incorrecta. Investigar si la sesión usa la declaración más completa.
No cambiar producción sin reproducir el fallo y registrar el alcance.
El fixture aceptó `refined()` pese al prototipo posterior `refined(int)`.
Permitir `src/Interop/Cpp/ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp`
y el fixture para seleccionar, entre redeclaraciones C de la misma entidad,
un prototipo disponible usando Clang. Comprobar ambos órdenes y proteger el
comportamiento C++ y las llamadas realmente sin prototipo.

## Funciones C con y sin prototipo

Preparar un fixture independiente para contrastar `f(void)` con la declaración
C antigua `f()`, retorno void, puntero void y promociones de argumentos sin
prototipo en Windows x64. Usar Clang como referencia y salidas en
`A:/nsc-interop-build`; no modificar producción antes de registrar un fallo
y el alcance de su corrección. Registrar resultados y controles negativos.
El fixture C11 confirmó que NSC rechaza una llamada válida a `old_style()` con
argumentos porque la proyecta como función de cero parámetros. Permitir
`CppFunctionDeclaration.h`, `ClangSession.cpp`, `CppSemanticFunction.cpp`,
los tests de sesión y el fixture para preservar si existe prototipo y emitir
llamadas C sin prototipo mediante la ruta de argumentos promovidos. Mantener
`f(void)` como cero argumentos y no extender esto a C++; verificar ABI y
metadata cuando aplique, con construcción y validación agrupadas.

## Actualizaciones volatile de campos C importados

La matriz mostró que Clang lee la unidad una vez para el valor previo y otra
antes del read-modify-write; NSC reutiliza la primera lectura. Preparar la
segunda lectura solo para campos volatile importados cuya procedencia Clang
esté presente, sin alterar campos nativos ni agregar atomics. Presupuesto:
`src/LLVM/Lowering/OopLlvmArraysAndStores.cpp`, fixture de matriz y registros.
Comparar accesos O0 e incrementar pruebas de prefijo, postfijo y asignación
compuesta; validar también las regresiones nativas. Una alineación LLVM menor
que la conocida por Clang es conservadora y se comprobará como tal, no se
confundirá con un cambio en el número de accesos observables.

## Matriz de unidades y operaciones de campos C volatile

Preparar un fixture independiente bajo `pruebas-tanda-08-bitfields-c` para
unidades de 8, 16, 32 y 64 bits, barreras de anchura cero y operaciones de
lectura, escritura e incremento. Comparar IR con Clang y ejecutar O0–O3.
Registrar fallos y resultados antes de cambiar producción; toda salida queda
en `A:/nsc-interop-build`.

## Unions y metadata de campos C volatile

Ampliar el fixture `pruebas-tanda-08-bitfields-c` para cubrir unions con
campos volatile superpuestos y bibliotecas precompiladas que publican structs
y unions de este tipo. Verificar ejecución O0–O3 y comparar el IR de funciones
equivalentes de Clang y NSC. No modificar producción sin una falla demostrada
y un alcance adicional registrado; conservar resultados en el tracking.

## Análisis de campos de bits C volatile

Preparar un fixture independiente en `pruebas-tanda-08-bitfields-c` para
contrastar acceso de C y NativeSharp a campos de bits volatile en structs
normales y empaquetados. Guardar IR de Clang en `A:/nsc-interop-build` y
documentar anchos, alineación y órdenes observados. No modificar producción
sin identificar el contrato efectivo y registrar su alcance.
La prueba rechaza los miembros volatile. Clang x64 MS registra unidades
discretas iniciadas por el primer campo de cada secuencia de almacenamiento;
para la muestra observada usa lecturas/escrituras volatile i32 alineadas a 4
o a 1 en el record empaquetado. Permitir `CppSemanticRecord.cpp` para calcular
esas unidades a partir de offsets y tamaños ya observados, y fixture/IR para
comparar. Mantener rechazo cuando los datos no permitan justificar el layout;
no atribuir la comparación de esta muestra a todos los objetivos o layouts.

## Campos de bits en unions C

Agregar un fixture independiente bajo `pruebas-tanda-08-bitfields-c` que
contraste lectura y escritura de miembros de bits superpuestos con miembros
enteros y bytes, con y sin signo, en O0–O3. Registrar resultado y alcance.
No modificar producción hasta identificar el fallo y registrar su presupuesto.
El fixture rechazó los miembros `first` y `delta` en O0. Permitir cambios en
`src/Interop/Cpp/CppSemanticRecord.cpp` para proyectar campos de bits no
volatile de union C dentro de su almacenamiento; conservar las comprobaciones
de límites y las restricciones para struct y C++. Ejecutar el fixture junto a
la prueba directa, metadata y regresiones existentes.

## Campos de bits C a través de biblioteca precompilada

Extender `pruebas-tanda-08-bitfields-c` con productor NativeSharp, consumidor
y script para verificar la reconstrucción del catálogo C y accesos a campos con
signo y empaquetados en O0–O3. Registrar evidencia en tracking y Markdown.
No cambiar producción hasta identificar y documentar una falla concreta.

## Observación de tipos C en bibliotecas precompiladas

Corregir la observación preliminar cuando coexisten una referencia a biblioteca
NativeSharp con catálogo C y una sesión Clang activa. Presupuesto: `src/Interop/Cpp/CppSemanticRecord.cpp`,
`tests/unit/CppClangSessionTests.cpp`, los fixtures existentes de `pruebas-tanda-08-meta-c`,
tracking y registros Markdown. Preservar el rechazo de identidades no conciliadas;
no suponer equivalencia por nombre ni USR. Verificar con construcción incremental
y pruebas dirigidas agrupadas antes de concluir cualquier soporte.

## Comparación de contratos de records C precompilados

Preparar comparación determinista de snapshots de records C entre módulos,
usando el codec existente, con controles de dimensiones, campos, calificadores,
destino y USR. Presupuesto: `src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp`,
`tests/unit/CppClangSessionTests.cpp`, tracking y registros Markdown. La
coincidencia de contratos no establece identidad nominal ni habilita conversiones;
el caso compartido permanece abierto. Validar junto con los cambios acumulados.

## Procedencia del buffer de declaraciones Clang

Capturar para declaraciones de record con ubicación directa la huella SHA-256
del buffer que Clang analizó, su nombre de archivo y offset. El nombre puede
corresponder a un archivo virtual, y no demuestra procedencia física. Presupuesto:
`include/nativesharp/Interop/CppRecordDeclaration.h`, `src/Interop/Cpp/ClangSession.cpp`,
`tests/unit/CppClangSessionTests.cpp`, tracking y registros Markdown. No leer el
archivo de nuevo ni atribuir ubicación directa a expansiones de macro. No
serializar, conciliar identidades ni habilitar conversiones con esta fase.

## Procedencia transitoria en snapshots C

Transportar el origen del buffer analizado al snapshot C de la definición,
únicamente si todas las definiciones observadas lo comparten. Presupuesto:
`include/nativesharp/Interop/CppRecordDeclaration.h`, `include/nativesharp/Interop/CppSemanticRecord.h`,
`src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp`,
tracking y registros. No cambiar aún el codec 1.42 ni tratar datos transitorios
como prueba de identidad. La serialización requiere compatibilidad explícita.

## Codec preparado de procedencia del catálogo C

Preparar un envoltorio versionado independiente del catálogo 1.42: conserva
el catálogo anterior como bloque y añade procedencia por record, validando
límites, orden, hash y canonicalidad. Presupuesto: `src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp`,
`tests/unit/CppClangSessionTests.cpp`, tracking y registros. No cambiar todavía
la versión NSMETA ni usar la extensión en bibliotecas hasta completar lectura,
escritura y compatibilidad de extremo a extremo. Validación agrupada.

## Lectura preparada de catálogos C versionados

Agregar selección explícita entre catálogo 1.42 y envoltura de procedencia;
permitir la segunda solo en metadata 1.43 o posterior. Presupuesto:
`src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp`,
`src/Interop/LibraryMetadataVerification.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`,
`src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp`, pruebas de sesión,
tracking y registros. No incrementar aún la versión escrita ni conectar el
productor. No asociar ni convertir identidades a partir de la envoltura.

## Activación versionada de procedencia en metadata C

Elevar la versión menor de metadata a 1.43 y emitir la envoltura de procedencia
solo al generar metadata 1.43; conservar lectura y escritura de 1.42. Presupuesto:
`include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`,
`src/Interop/LibraryMetadataConstruction.cpp`, `src/Interop/LibraryMetadataPayload.cpp`,
`src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp`,
`tests/unit/CppClangSessionTests.cpp`, fixtures existentes de metadata C, tracking
y registros. Probar ambos formatos y el productor/consumidor agrupados. La
procedencia no debe cambiar la huella ABI pública; la integridad de toda la
metadata sigue protegida por su hash. La activación no prueba que declaraciones
de módulos distintos sean equivalentes.

## Evidencia común de declaración C entre módulos

Preparar una comprobación compartida de origen y contrato del record raíz:
archivo existente equivalente, huella del buffer, offset y contrato capturado.
Presupuesto: `src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp`,
`tests/unit/CppClangSessionTests.cpp`, tracking y registros. El resultado no
certifica dependencias transitivas ni habilita conversiones; la conciliación
integral debe comprobar también records y enums dependientes. Conservar errores
y estados insuficientes sin alterar salidas. Validación agrupada.

## Cierre de contratos de records C

Preparar comparación de records, campos y enums alcanzables entre catálogos C,
con detección de ciclos, dependencias ausentes y diferencias de contrato.
Presupuesto: `src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp`,
`tests/unit/CppClangSessionTests.cpp`, tracking y registros. No unir todavía
identidades ni habilitar conversiones; comprobar el origen del record raíz y
de cada record dependiente. Mantener errores y salidas transaccionales.

## Mapa de declaraciones C reconciliables visibles

Construir un mapa entre records de sesiones activas y catálogos precompilados
visibles, solo cuando coincidan procedencia y cierre comprobado. Presupuesto:
`include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`,
`tests/unit/CppClangSessionTests.cpp`, tracking y registros. Múltiples candidatos,
dependencias ausentes y origen no comprobable no deben producir una equivalencia.
El mapa se prepara sin alterar aún la búsqueda ni permitir conversiones. Agrupar
la validación con la conexión a resolución cuando esté implementada.

## Transporte de identidades C reconciliadas

Transportar el destino de cada reconciliación comprobada en la declaración C
importada, tanto en observación como en el selector final. Presupuesto:
`include/nativesharp/Frontend/ProjectNameLookup.h`,
`src/Frontend/ProjectNameLookup.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`, controles de sesión, tracking y
registros. Esta fase no cambia todavía búsqueda, conversiones ni semántica;
validar destinos antes de usarlos en la siguiente fase.

## Resolución de nombres C reconciliados

Conectar únicamente identidades C previamente comprobadas al índice de nombres,
revisar visibilidad, unificar candidatos y conservar la observación del grupo
Clang. Presupuesto: `include/nativesharp/Frontend/ProjectNameLookup.h`,
`src/Frontend/ProjectNameLookup.cpp`, `src/Frontend/NameLookup/TypeVisibility.cpp`,
`src/Frontend/NameLookup/QualifiedNamespaces.cpp`,
`src/Frontend/NameLookup/QualifiedTypeNames.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`,
controles de sesión, tracking y registros. No deducir equivalencias por nombre
ni extenderlas a C++; la proyección de firmas y ABI aún debe conectarse y probarse.

## Firmas de funciones C reconciliadas

Proyectar los records C reconciliados al ordinal canónico en firmas y conservar
sus grupos Clang para verificar ABI y USR. Presupuesto:
`include/nativesharp/Frontend/ProjectNameLookup.h`, `src/Frontend/ProjectNameLookup.cpp`,
`src/Interop/Cpp/CppSemanticFunction.cpp`, `src/Interop/Cpp/CppSemanticFunctionGroups.cpp`,
controles y fixtures existentes de metadata C, tracking y registros. No alterar
firmas C++ ni aceptar conversiones sin la reconciliación previamente validada.
Validación agrupada pendiente.

## Variables locales de enums C importados

Corregir la barrera de vida de objetos que rechaza un enum C completo al
inferir un local. Presupuesto: `src/Frontend/Bodies/StatementAnalyzer.cpp`,
`src/LLVM/Lowering/OopLlvmStatements.cpp`, fixtures existentes de metadata C,
tracking y registros. Mantener la barrera para C++ y enums sin representación
validada. La conciliación de enums y enumeradores entre módulos sigue aparte;
agrupar su validación con el resto de correcciones de la tanda.

## Enums C compartidos vinculados a records reconciliados

Reconciliar enums alcanzados por un record ya comprobado y sus enumeradores,
conservando tipo subyacente y valores. Presupuesto:
`src/Interop/Cpp/CppSemanticRecord.cpp`, `src/Frontend/ProjectNameLookup.cpp`,
`include/nativesharp/Interop/CppSemanticEnumType.h`,
`include/nativesharp/Frontend/NamespaceStateSymbol.h`, `src/Frontend/Bodies/IdentifierAnalyzer.cpp`, fixtures y controles de
sesión, tracking y registros. No equiparar enums por nombre o USR aislados;
el alcance de este paso depende del cierre del record validado. Enums no
alcanzados por ese cierre conservan la resolución previa. Validación agrupada.

## Comparación numérica de enum C importado

Permitir comparaciones de un enum C completo con operandos numéricos usando
su representación validada y las reglas numéricas de NativeSharp; conservar
ancho y signo al emitir conversiones. Presupuesto:
`include/nativesharp/Frontend/CppImportedEnumNumeric.h`,
`src/Frontend/Bodies/OperatorExpressionAnalyzer.cpp`,
`src/Frontend/HIR/HirExpressionMachine.cpp`, fixtures de metadata C, tracking
y registros. No alterar enums nativos ni modificar asignaciones generales.
La validación debe abarcar signos, anchuras y fases semántica/LLVM agrupadas.

## Identidad de funciones C importadas

T08-002/T08-003: conservar USR opcional en la proyección de función y
conciliarlo en declaraciones agrupadas del mismo nombre y símbolo; rechazar
USR conocidos contradictorios sin confundir alias de nombres diferentes.
Presupuesto: NamespaceFunctionSymbol.h, CppSemanticFunction.cpp,
CppSemanticFunctionGroups.cpp, suite de sesión y registros Markdown.
No modifica formato persistente, LLVM externo ni NSLink; validar por tanda.

## Captura persistible de records C

T08-002/T08-010/T08-011/T08-013: preparar una captura propia de record C con
identidad, destino, completitud, dimensiones y tipos/campos observados por
Clang; rechazar grupos incompatibles sin fingir persistencia. Presupuesto:
CppSemanticRecord.h/.cpp, CppClangSessionTests.cpp, TRACKING.md y registros
Markdown. No altera todavía el formato, LLVM ni NSLink; validación agrupada.
Conservar también identidades USR conocidas de tipos anidados de cada campo,
incluyendo elementos, tipos subyacentes y firmas de función, cuando la primera
observación compatible no las tenga. No modificar los árboles originales.
Preparar la recopilación de grupos C seleccionados en un catálogo ordenado por
identidad: rechazar duplicados o una captura fallida sin producir salida parcial.

## Codificación interna de tipos C para metadata

T08-002/T08-010/T08-011/T08-013: preparar un codec binario interno del catálogo
de records C que preserve los árboles de tipos, calificadores, identidades,
offsets y dimensiones, con límites de profundidad y validación de entradas.
Presupuesto inicial: src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp,
CMakeLists.txt, controles de codec y registros Markdown. No cambiar todavía la
versión NSMETA ni la publicación hasta integrar verificación, huella ABI,
fusión y reconstrucción; la configuración sin Clang debe seguir disponible.
Preservar también el modelo de volatile observado, sin inferirlo cuando las
observaciones discrepen: CppSemanticRecord.h/.cpp y controles de sesión.
Preparar el almacenamiento del catálogo codificado en el modelo de metadata,
su preservación en fusión y poda, y el rechazo explícito al intentar escribirlo
en versiones anteriores a 1.42. Presupuesto adicional: LibraryMetadata.h,
LibraryMetadataMerger.cpp, LibraryMetadataVerification.cpp,
LibraryMetadataCodec.cpp y PrecompiledSurfaceReachability.cpp. No habilitar
todavía 1.42 ni publicar el catálogo hasta completar su lectura y huella ABI.
Preparar en LibraryMetadataPayload.cpp el campo delimitado para 1.42 y su
lectura acotada, además de incorporarlo a la proyección ABI cuando exista.
Mantener 1.42 deshabilitado como versión pública hasta conectar reconstrucción.
Preparar en CppSemanticRecord.h/.cpp la selección de referencias de la API y
su cierre para el proveedor; no instalarlo aún. Reportar enums sin catálogo en
vez de producir metadata incompleta; incluir controles de sesión.
Preparar la captura de enumeraciones C requeridas, con tipo subyacente,
valores exactos e identidades reconciliadas; archivos CppSemanticInventory.h/.cpp
y controles de sesión. No sustituir todavía el rechazo del proveedor hasta
disponer de codec y reconstrucción de enums.
Ampliar el codec interno para transportar conjuntamente records y enums C;
los accesos exclusivos de records deben rechazar enums en vez de descartarlos.
Presupuesto: CppCRecordMetadataCodec.h/.cpp, LibraryMetadataVerification.cpp y
controles de sesión. El formato 1.42 permanece deshabilitado.
Conservar y verificar la identidad estructurada de cada record C (módulo,
namespace, contenedores y nombre) para reconstruirla sin analizar cadenas.
Presupuesto adicional: CppSemanticRecord.h/.cpp y controles de sesión.
Conservar además si el layout permite valores o solo acceso mediante puntero,
para no elevar una evidencia de definiciones repetidas a compatibilidad por valor.
Preparar la reconstrucción de declaraciones de tipos C y enum para un módulo y
ordinal de proyecto explícitos, a partir del catálogo validado. Presupuesto:
CppCRecordMetadataCodec.h/.cpp y controles de sesión. Campos y constantes se
reconstruirán por separado; no conectar todavía al consumidor ni habilitar 1.42.
Preparar la proyección de campos y enumeradores guardados reutilizando las
reglas C existentes; rechazar datos no proyectables sin salida parcial.
Presupuesto: CppSemanticRecord.h/.cpp y controles de sesión. No instalar aún
en el consumidor hasta resolver visibilidad y colisiones.
Preparar combinación transaccional de campos y constantes de catálogos
precompilados, comprobando propietarios y duplicados; archivos
CppSemanticRecord.h/.cpp y controles de sesión. No activar todavía el formato.
Preparar un selector que combine tipos de sesiones Clang y de catálogos
precompilados, rechazando identidades compartidas hasta demostrar su equivalencia
de declaración completa. No conectarlo todavía al driver ni habilitar 1.42.
Preparar el proveedor de catálogo para capturar enums de firmas y dependencias
de campos, ahora que existen el codec y la reconstrucción por separado. Presupuesto:
CppSemanticRecord.cpp y controles de sesión; conservar el proveedor sin instalar,
el formato 1.42 deshabilitado y rechazar las definiciones ausentes o conflictivas.
Preparar proveedores combinados para campos y valores provenientes de sesiones
y de catálogos precompilados: filtrar enumeradores por demanda y visibilidad,
detectar colisiones y preservar las salidas si falla la reconstrucción. Presupuesto:
CppSemanticRecord.h/.cpp y controles de sesión. No conectarlos aún al driver.
Preparar un control integrado en CppClangSessionTests.cpp que instale un catálogo
en un módulo precompilado sintético y configure selector y proveedores combinados
en ProjectCompilation. Comprobar resolución de tipo, campo y enumerador y una
declaración NativeSharp de la superficie. No habilitar el formato ni el driver.
Preparar la verificación de referencias NSMETA para reconocer como locales los
tipos C de un catálogo validado y exigir la dependencia existente para tipos de
otros módulos, sin permitir colisiones con tipos NativeSharp o especializaciones.
Presupuesto: LibraryMetadataVerification.cpp y controles de sesión; conservar
1.42 y el proveedor deshabilitados hasta la integración y validación conjunta.
Extender el cierre de tipos externos entre bibliotecas para reconocer un tipo C
presente en el catálogo validado del módulo dueño, conservando el rechazo de
proveedor ausente o tipo no declarado. Presupuesto: MetadataSourceMapping.cpp,
controles unitarios de interoperabilidad y registros. No habilitar 1.42.
Conectar en NSC la producción y consumo de catálogos C mediante NSMETA 1.42:
actualizar el máximo de formato, instalar el proveedor del productor para sesiones
C/C++, y configurar selector, campos y valores combinados cuando se cargue un
catálogo precompilado. Presupuesto: LibraryMetadata.h,
PrecompiledModuleConfiguration.cpp, pruebas de metadata/sesión y fixture
pruebas-tanda-08-meta-c, tracking y registros. Mantener el rechazo de identidades
activas/precompiladas aún no conciliadas y validar la ruta real antes del cierre.
Añadir ese fixture a validar-tanda-08.cmd para que la batería agrupada registre
su resultado junto con los demás casos de la tanda.
Ampliar pruebas-tanda-08-meta-c para publicar un enum C en una firma y en un
campo de record, consumir su constante y contrastar las cuatro optimizaciones
O0–O3 en directorios separados. Mantener los artefactos en A: y NSLink protegido.
En el mismo fixture agregar un retorno C de record por valor, publicarlo en la
API NativeSharp y verificar sus campos en el consumidor; no inferir por ello
compatibilidad por valor de records arbitrarios.
Implementar proyección por valor para funciones de enlace C cuando el record C
tiene definición única, layout completo instalado y tamaño/alineación coincidentes
con Clang. Mantener rechazo de records C++ y definiciones múltiples no probadas.
Presupuesto: CppSemanticFunction.cpp, controles de sesión, fixture de metadata C
y registros. Comprobar después ABI directo e indirecto con Clang en conjunto.
Propagar a MIR los layouts de records C completos que ya tienen objectLayout
verificado y admitirlos en el cálculo de layout nominal, sin habilitar records
C++ ni registros incompletos o de acceso exclusivamente por puntero. Presupuesto:
ProjectObjectLayoutModel.cpp, MirLayoutImporter.cpp, controles de sesión y
fixture de metadata C. Preservar offsets de campos ya validados y probar ABI.
Permitir variables locales por valor únicamente para struct/union C con layout
completo instalado; mantener el rechazo previo para C++ e incompletos. Presupuesto:
StatementAnalyzer.cpp, controles existentes y fixture de metadata C.
En OopLlvmStatements.cpp usar la inicialización de valor ordinaria para locales
struct/union C con layout validado; conservar construcción in situ para C++ y
el rechazo de tipos incompletos. Verificarlo con el fixture directo e indirecto.
Agregar al fixture de metadata un proyecto C directo que use `var` y llamadas
con retorno y parámetro record sin escribir los nombres de esos tipos en NS;
ejecutarlo en O0–O3 con los demás casos para comprobar inferencia y selección.
Agregar al fixture un consumidor que combine la LibraryReference y su propio
CppLibraryReference del mismo encabezado C; verificar asignación y lectura de
punteros obtenidos de ambas fuentes. Conservarlo en la matriz como prueba real.
Transportar los bytes verificados del catálogo desde la superficie de metadata
hasta PrecompiledSyntaxModule; presupuesto adicional: PrecompiledSyntaxModule.h,
PrecompiledProjectPreparation.cpp y controles existentes. No habilitar 1.42
ni instalar declaraciones en este paso.
Preparar reconstrucción de declaraciones desde todos los módulos precompilados,
resolviendo su módulo propietario en el grafo y rechazando duplicados
incompatibles. Presupuesto: CppCRecordMetadataCodec.h/.cpp y controles de
sesión; no instalar todavía en el frontend ni habilitar el formato.
Preparar un proveedor opcional de catálogo para el productor de metadata, sin
activarlo desde el driver. Presupuesto adicional: ProjectCompilation.h/.cpp,
LibraryMetadataConstruction.cpp y controles de sesión. El productor sin
proveedor mantiene su comportamiento y una salida fallida no deja catálogo
parcial; 1.42 continúa rechazado por el escritor público hasta su integración.
Preparar cierre transitivo de grupos record C solicitados mediante identidades
de campos y tipos anidados; conservar las identidades enum requeridas como
pendientes para su propio catálogo. Rechazar dependencias faltantes o ambiguas.
Presupuesto adicional: CppSemanticRecord.h/.cpp y controles de sesión.

## Procedencia y destino de tipos importados

T08-002/T08-010/T08-011/T08-013: conservar en el tipo importado si Clang lo
observó como struct, union o enum C, o como record C++, y el triple consensuado.
Sin inventar procedencia en grupos contradictorios. Presupuesto:
ProjectNameLookup.h, CppSemanticRecord.cpp, CppSemanticInventory.cpp,
CppClangSessionTests.cpp, TRACKING.md y registros Markdown. No modifica el
formato persistente todavía, LLVM ni NSLink; validación agrupada.

## Comprobación agrupada de visibilidad e identidad

Construcción incremental de NSC y la suite de sesión ON en A:/nsc-interop-build;
ejecutar la suite y los fixtures de matriz y sombras de pruebas-tanda-08-meta-c.
Registrar evidencia en TRACKING.md, TANDA-08-TIPOS-C.md y registro de commits.
No es la validación completa de la tanda; LLVM y NSLink no se modifican.

## Funciones C/C++ globales desde namespace NativeSharp

T08-003/T08-004/T08-009: incluir el ámbito global en la demanda y usar sus funciones importadas
como última opción si no hay candidatos léxicos ni importados por using. Mantener
el comportamiento de funciones NativeSharp globales y las prioridades existentes.
Aplicar la misma prioridad a globales C y constantes de enumerador importadas.
Presupuesto: ProjectNamespaceFunctionModel.cpp, pruebas-tanda-08-meta-c/*,
CppClangSessionTests.cpp, TRACKING.md y registros Markdown; validación agrupada.

## Superficie pública de biblioteca con tipo C

T08-002/T08-010/T08-013: preparar un fixture de productor NativeSharp con
tipo C en firma pública y consumidor mediante LibraryReference. Presupuesto:
pruebas-tanda-08-meta-c/*, TRACKING.md y registros Markdown; ejecución posterior
en A:/nsc-interop-build/tanda08-meta-c. LLVM y NSLink permanecen protegidos.
Incluir una matriz de diagnóstico de llamadas C escalares y con retorno de
puntero, en ámbito global y namespace, sin reconstruir el compilador.

## Comprobación incremental de cambios acumulados

T08-002/T08-006/T08-009: construir incrementalmente los objetivos NSC y suite
de sesión ON en A:/nsc-interop-build; ejecutar la suite y el fixture de layout
para los cambios acumulados de identidad, enums y `_Bool`. Registrar evidencia
en TRACKING.md, TANDA-08-TIPOS-C.md y registro de commits. No es la validación
completa de la tanda; LLVM, NSLink y los insumos de VC permanecen sin modificar.

## Identidad de campo con observación inicial incompleta

T08-002: conciliar USR de campo entre todas las definiciones completas, incluso
cuando la primera no lo reporte. Presupuesto: CppSemanticRecord.cpp,
CppClangSessionTests.cpp, TRACKING.md y registros Markdown. No cambia LLVM,
NSLink ni persistencia; validación agrupada.
Incluir el control integrado que recupera el USR en el símbolo de campo.
Comparar también identidades conocidas de tipos record y enum anidados entre
todas las observaciones compatibles, con prueba de ausencia inicial.

## Layout C de booleanos

T08-006/T08-010/T08-016/T08-020: añadir al fixture de layout C campos y
arreglos `_Bool`, tamaños, offsets, llamadas y lectura/escritura mixta. Presupuesto:
pruebas-tanda-08-layout/Api.h, Api.c, Main.ns, TRACKING.md y registros
Markdown. No se modifica LLVM ni NSLink; la ejecución es por tanda agrupada.

## Identidad de enumeradores C

T08-002: conservar y conciliar USR conocidos de enumeradores C al proyectar y
seleccionar constantes y al conciliar sus definiciones enum. Presupuesto: CppSemanticInventory.h/.cpp,
CppClangSessionTests.cpp, TRACKING.md y registros Markdown. No cambia LLVM,
NSLink ni formatos persistentes; validación agrupada por tanda.
T08-009: al comparar definiciones enum, preservar también el tipo efectivo de
cada enumerador, no solo su nombre y valor, dentro del mismo presupuesto.
Conciliar también el triple de destino de las observaciones de una constante,
para evitar unir declaraciones provenientes de ABI distintos.

## Identidad de alias C en resolución

T08-002: conciliar los USR conocidos de observaciones del mismo alias al
resolverlo, incluso si una observación intermedia no tiene USR. Presupuesto:
QualifiedTypeNames.cpp, CppClangSessionTests.cpp, TRACKING.md y registros
Markdown. No cambia persistencia ni LLVM o NSLink; validación por tanda.

## Identidad de global C en selección y símbolo

T08-002/T08-004: conservar el USR opcional de declaraciones globales C
seleccionadas hasta NamespaceStateSymbol; rechazar USR conocidos y distintos
en el mismo grupo sin inventar uno ausente. Presupuesto:
CppSemanticInventory.h/.cpp, NamespaceStateSymbol.h, suite de sesión y
registros Markdown. No altera formato persistente. LLVM y NSLink protegidos;
validación agrupada de la tanda.

## Selección de campos con USR contradictorio

T08-002: al seleccionar definiciones repetidas de un record, rechazar campos
correspondientes cuyos USR Clang estén presentes y discrepen. Mantener las
comprobaciones existentes y no inventar identidad cuando falte un USR.
Presupuesto: CppSemanticRecord.cpp, CppClangSessionTests.cpp y registros
Markdown. LLVM externo y NSLink protegidos; validación agrupada de la tanda.

## Identidad Clang de campo C importado

T08-002: conservar el USR opcional capturado por Clang al proyectar un campo
de instancia hasta el metadato importado, sin usar su ausencia como identidad
inventada ni afirmar persistencia completa. Presupuesto: CppSemanticRecord.h,
CppSemanticRecord.cpp, CppImportedInstanceField.h, suite de sesión y registros
Markdown. LLVM externo y NSLink protegidos; validación agrupada por tanda.
También se transporta el USR al ClassMemberSymbol mediante ProjectMemberModel;
persistencia y selección por identidad permanecen pendientes.

## Orden de enum volatile C alineado en arreglo de campo

T08-015: en índices de arreglos fijos de campos C importados, reconocer un
elemento enum por su identidad y layout importados y conservar su ancho cuando
el modo MSVolatile efectivo está acreditado. Emitir acquire en lectura y
release en escritura solo si la alineación efectiva del elemento alcanza ese
ancho. Mantener accesos no atómicos de elementos subalineados y del campo
entero anidado observado en Clang; no inferir el modo ni aplicarlo a punteros
cargados. Presupuesto: MirNodeMachine.cpp, OopLlvmSupport.h,
OopLlvmExpressionMachine.cpp, OopLlvmArraysAndStores.cpp, PackedArrayAlignment.h, fixture layout y
registros Markdown. LLVM externo y NSLink protegidos; validar por tanda.

## Modo volatile de campo y elemento en HIR

T08-015: conservar el modo efectivo del campo importado en sus nodos HIR y
propagarlo por direcciones de arreglos fijos, sin transferirlo desde el valor
de un puntero cargado hacia su apuntado. No habilita aún operaciones atómicas.
Presupuesto: HirNameExpressionLowering.cpp, HirCompletionValueFrames.cpp,
HirCompletionAccessFrames.cpp y registros Markdown. LLVM externo y NSLink
protegidos; validación agrupada.

## Modo volatile en campos importados

T08-015: transportar el modo efectivo desde observaciones de record hasta el
campo y su símbolo, conservando desconocido cuando una observación del grupo
no coincide o carece de dato. No cambia aún HIR, MIR ni código generado.
Presupuesto: CppImportedInstanceField.h, ClassMemberSymbol.h,
CppSemanticRecord.cpp, ProjectMemberModel.cpp y registros Markdown. LLVM
externo y NSLink protegidos. Validación agrupada de la tanda.
La suite CppClangSessionTests.cpp también puede comprobar la procedencia del
modo en el símbolo de campo; no habilita generación de código por sí misma.

## Modo volatile Clang en inventario de records

T08-015: conservar la opción efectiva MSVolatile reportada por cada sesión
en cada observación de record, sin inferirla del triple. Solo inventario;
no modifica la semántica emitida ni resuelve conflictos entre sesiones.
Presupuesto: CppSemanticRecord.h/.cpp, CppClangSessionTests.cpp y registros Markdown. LLVM externo y
NSLink protegidos. Construcción y validación agrupadas de la tanda.

## Comparación de volatile C alineado en agregados

T08-015/T08-016: contrastar el orden y alineación que emiten Clang y NSC al
acceder a un arreglo de records y enums volatile alineados naturalmente. Usar
el fixture layout, su verificador de IR y registros Markdown; no inferir la
semántica de un campo alineado a partir de un ejemplo empaquetado. NSLink y
LLVM externo protegidos; validación agrupada.

## Arreglos volatile C de enums y records

T08-015/T08-016: proyectar arreglos C de enums o records completos volatile,
propagar también const y preservar accesos volatile y alineación en sus campos
y elementos. No perder calificadores al tomar direcciones. Presupuesto:
CppSemanticRecord.cpp, fixture layout, verificador de IR y registros Markdown;
LLVM externo, NSLink y ruta C++ protegidos. Validación agrupada por tanda.

## Arreglos const C de enums y records

T08-015/T08-016: proyectar arreglos C de enums o records completos const,
conservando readonly para impedir escrituras y direcciones que pierdan const.
Mantener el rechazo de volatile y las comprobaciones de identidad y layout.
Presupuesto: CppSemanticRecord.cpp, fixture layout y registros Markdown;
LLVM externo, NSLink y la ruta C++ protegidos. Validación agrupada por tanda.

## Campos C empaquetados de enums y arreglos de records o enums

T08-009/T08-010/T08-012/T08-016: proyectar campos C subalineados que sean
enums y arreglos fijos de enums o records completos, sin alterar la ruta C++.
Conservar las comprobaciones de identidad, tamaño y alineación del proyector;
respetar la alineación al acceder y evitar que escapen direcciones
subalineadas. Esta entrada amplía las restricciones históricas de las entradas
inferiores para estas formas concretas, sin borrarlas. Presupuesto:
CppSemanticRecord.cpp, fixture layout, verificador de IR, tracking y registros
Markdown. LLVM y NSLink protegidos; construcción incremental y validación
agrupada. Volatile, tipos incompletos y demás formas no representadas siguen
fuera de esta ampliación.

## Records C anidados subalineados

T08-010/T08-012: proyectar campos record C completos y compatibles aunque
su alineación efectiva sea menor que la natural; conservar los controles de
identidad, tamaño y layout del tipo interior. Mantener barreras de C++,
record volatile y demás formas aún no representadas. Presupuesto:
CppSemanticRecord.cpp, fixture layout y registros Markdown; LLVM externo y
NSLink protegidos. Validación agrupada por tanda.

## Protección de direcciones de records C anidados

T08-010/T08-012: identificar por su layout natural un campo record C importado
subalineado e impedir que su dirección o la de sus campos interiores escape
como puntero ordinario. No propagar la restricción desde el almacenamiento de
un puntero hacia su objeto apuntado. No habilitar todavía el proyector de
records subalineados. Presupuesto: MemberAccessAnalyzer.cpp, fixture layout y
registros Markdown. LLVM externo y NSLink protegidos; validación agrupada.

## Alineación de campos y arreglos anidados

T08-010/T08-012/T08-016: combinar la alineación MIR de un campo con la de
direcciones de campos y elementos de arreglo que forman su receptor por valor;
no propagar alineación a través de un puntero cargado. Aplicar a accesos y
conservar test de procedencia sin campo. No habilitar aún records anidados
subalineados. Presupuesto: PackedArrayAlignment.h privado, dos unidades LLVM
de acceso, CppClangSessionTests.cpp y registros Markdown. LLVM externo y
NSLink protegidos; validación agrupada por tanda.

## Arreglos C empaquetados de punteros representables

T08-014/T08-016: permitir arreglos fijos C subalineados de punteros que el
mapeador de arrays ya representa sin perder calificadores. Mantener la
alineación del almacenamiento al indexar y proteger la dirección de un slot
subalineado; no cambiar la alineación del objeto apuntado. No habilitar
punteros a const/volatile ni arreglos de records o enums subalineados.
Presupuesto: CppSemanticRecord.cpp, fixture layout y registros Markdown;
LLVM y NSLink protegidos. Validación agrupada por tanda.

## Arreglos escalares C empaquetados const y volatile

T08-015/T08-016: conservar const y volatile de todos los niveles de arreglos
escalares fijos C en el campo importado y su acceso, incluida alineación
reducida. Mantener restringidos arreglos subalineados de punteros, records,
enums y C++, y no alterar aliases generales. Presupuesto:
CppSemanticRecord.cpp, fixture layout y registros Markdown. LLVM y NSLink
protegidos; validación agrupada por tanda.

## Arreglos escalares C empaquetados

T08-010/T08-012/T08-016: permitir arreglos fijos C subalineados solo si todos
sus niveles son completos y no calificados y el elemento final es escalar
booleano, entero o flotante. Conservar la alineación efectiva hasta LLVM y
proteger dirección del arreglo y sus elementos. Mantener rechazados arreglos
de punteros, records, enums, calificados o C++ cuando estén subalineados.
Presupuesto: CppSemanticRecord.cpp, MemberAccessAnalyzer.cpp, fixture layout,
registros Markdown. LLVM y NSLink protegidos; validación agrupada.

## Dirección de elementos de arreglos C empaquetados

T08-016: en la indexación fija de un campo importado, impedir que el elemento
sea expuesto por dirección o referencia cuando la alineación efectiva del
campo sea inferior a la natural del elemento escalar o puntero. Conservar los
accesos directos y las demás clases de índices. El proyector mantiene la
barrera para arreglos empaquetados hasta preparar el circuito completo.
Presupuesto: IndexAndRangeExpressions.cpp, fixture layout y registros Markdown.
LLVM y NSLink protegidos; validación agrupada de la tanda.

## Alineación de elementos de arreglo en un campo

T08-010/T08-012/T08-016: derivar en LLVM la alineación garantizada de un
elemento indexado directamente desde un campo de arreglo, incluyendo rangos
multidimensionales, mediante la alineación MIR del campo y los strides.
Aplicarla en cargas y escrituras simples y compuestas. No habilitar todavía
arreglos empaquetados ni alterar indexación de punteros que no provienen de
FieldAddress. Presupuesto: nuevo PackedArrayAlignment.h privado, las dos
unidades LLVM que realizan los accesos, fixtures y registros Markdown. LLVM
externo y NSLink protegidos; pruebas agrupadas por tanda.

## Optimizaciones del fixture de layout C

T08-010/T08-012/T08-020: ejecutar el proyecto mixto de layout también en O1,
O2 y O3, conservando el análisis IR O0 y los casos negativos. No reconstruir
NSC entre opciones. Presupuesto: validar.cmd del fixture layout y registro
Markdown; la batería completa continúa pendiente.

## Comprobación ampliada de campos C con pack(2)

T08-010/T08-012/T08-020: ampliar el fixture existente con campos short,
int y double bajo `pack(2)`, comparar layout y valores con C, exigir IR con
alineación 2 y comprobar que un campo short naturalmente alineado conserva su
dirección utilizable. Presupuesto: fixture pruebas-tanda-08-layout y registros
Markdown. Sin cambios de producción ni compilación completa por ítem.

## Campo puntero C empaquetado

T08-010/T08-012/T08-014: permitir la proyección de un campo puntero C ya
representable aunque su almacenamiento esté subalineado. La carga/escritura
del puntero usa alineación del campo; el objeto apuntado conserva su propio
contrato. Impedir exponer la dirección del campo puntero como `T**` mientras
NSC no preserve alineación de esa dirección. No habilitar agregados, bitfields,
punteros a volatile ni campos C++ empaquetados. Presupuesto:
CppSemanticRecord.cpp, MemberAccessAnalyzer.cpp, fixture layout y registros
Markdown. LLVM y NSLink protegidos; validación agrupada con la tanda.

## Campos escalares C empaquetados

T08-010/T08-012/T08-020: habilitar acceso directo a campos escalares C cuya
alineación efectiva sea menor que la natural, calculada con alineación del
record y offset de Clang. No habilitar campos empaquetados C++, agregados,
punteros ni bitfields. Impedir exponer como puntero ordinario la dirección de
un campo escalar desalineado; preservar acceso directo, const y volatile.
Presupuesto: CppSemanticRecord.cpp, MemberAccessAnalyzer.cpp, fixture de layout,
CppClangSessionTests.cpp y registros Markdown. NSLink y LLVM protegidos;
validación dirigida agrupada y después batería al cerrar la tanda.

## Transporte de alineación efectiva importada

T08-010/T08-012/T08-020: conservar `byteAlignment` de un campo importado en
el símbolo y usarla en el layout importado con comprobación de potencia de
dos, límites y divisibilidad. La semántica de `align(N)` de campos NativeSharp
permanece intacta. El importador sigue rechazando campos empaquetados hasta
resolver direcciones escapadas y accesos indirectos. Presupuesto:
ClassMemberSymbol.h, ProjectMemberModel.cpp, ProjectObjectLayoutModel.cpp,
CppClangSessionTests.cpp y registros Markdown. LLVM y NSLink protegidos;
validación agrupada al cierre de la tanda.

## Verificador de bloques de coalescencia

La prueba dirigida llegó al IR esperado, pero el verificador no acepta los
nombres de bloque `coalesce.store.fallback.193` que NSC genera. Ajustar solo
su patrón para el punto y número observados; conservar la exigencia de dos
bloques con escritura release y dos lecturas acquire. Presupuesto:
verificar_ir_coalescencia.py y registros Markdown; repetir el caso agrupado.

## Diagnóstico de coalescencia sobre global C volatile

El caso mixto de `??=` sobre `volatile_record_pointer` falló antes del IR:
el análisis usa `addressable` tanto para impedir que una dirección volatile
escape como para una dirección interna de coalescencia. Permitir únicamente
un identificador modificable de almacenamiento volatile con tipo puntero y
apuntado sin calificadores, manteniendo `addressable` falso y el rechazo de
`&` y de propiedades. Presupuesto: AssignmentAnalyzer.cpp, fixture existente
y registros Markdown. No modificar LLVM ni NSLink; repetir los casos afectados
en grupo tras el build incremental.

## Alineación explícita en accesos directos a campos

T08-010/T08-012/T08-020: MIR ya conserva la alineación del layout del campo,
pero las cargas y escrituras ordinarias directas no la indican a LLVM. Emitir
esa alineación para FieldLoad y FieldAddress en lectura, escritura simple y
compuesta, sin habilitar todavía campos C empaquetados. Preservar los demás
destinos, bitfields y orden volatile. Presupuesto: OopLlvmExpressionMachine.cpp,
OopLlvmArraysAndStores.cpp, fixture de layout C y registros Markdown. LLVM y
NSLink protegidos; validación incremental agrupada al cierre de la tanda.

## Coalescencia sobre puntero global C volatile

T08-015: `??=` admite almacenamiento puntero, pero `lowerNullCoalescingStore`
emite cargas y escrituras volatile sin consultar el orden MS ya acreditado para
el global importado. Reutilizar la comprobación existente de tipo, opción y
alineación y preservar el flujo condicional y las otras clases de destino.
Presupuesto: OopLlvmConditionalsAndNullAware.cpp, fixture
pruebas-tanda-08-record-c y registros Markdown. LLVM y NSLink protegidos;
validación agrupada al cierre de la tanda.

## Globales C volatile de punto flotante

T08-007/T08-015: Clang 23.1.0 en Windows x64 emite `load atomic volatile
float acquire` y `store atomic volatile float release` para globales float
naturales. La ruta de NSC solo atribuye este orden a enteros y punteros.
Extenderla a float de 32 y double de 64 bits únicamente cuando Clang confirme
modo MS, tamaño y alineación suficiente. Preservar las rutas sin esas garantías.
Presupuesto: HirNameExpressionLowering.cpp, OopLlvmSupport.h, fixture
pruebas-tanda-08-record-c y registros Markdown. LLVM y NSLink protegidos;
construcción incremental y validación agrupada de la tanda.

## Cierre transitivo de aliases de globales

T08-002/T08-004: la relación entre globales por símbolo o USR puede tener más
de un salto. Recorrerla dentro del proyecto antes de comparar contratos para
que una contradicción indirecta no quede oculta. Preservar las observaciones
internas fuera de la selección externa y evitar duplicados. Presupuesto:
CppSemanticInventory.cpp, CppClangSessionTests.cpp y registros Markdown.
LLVM y NSLink protegidos; validación agrupada por tanda.

## Globales con identidad Clang contradictoria

T08-002/T08-004: una demanda de global debe detectar declaraciones del mismo
proyecto y USR conocido aunque difieran nombre o símbolo, y comprobar su
contrato existente. Preservar aliases compatibles, identidad desconocida,
aislamiento entre proyectos, alineación y opciones volatile. Presupuesto:
CppSemanticInventory.cpp, CppClangSessionTests.cpp y registros Markdown.
LLVM y NSLink protegidos; validación completa agrupada al terminar la tanda.

## Cierre de dependencias de campos por USR

T08-002/T08-010: al recorrer dependencias transitivas de campos C, no resolver
un record o enum por nombre si ambos USR son conocidos y difieren, incluso si
el tipo ya fue seleccionado. Conservar controles de layout y conflictos y la
ruta existente con USR ausente. Archivos: CppSemanticRecord.cpp,
CppClangSessionTests.cpp y registros Markdown. Validación agrupada por tanda.

## USR de record en proyección de firmas sin lookup

T08-002/T08-003: la proyección aislada de funciones y métodos conserva solo
el estado del grupo y pierde su USR cuando no recibe ProjectNameLookup.
Mantener acceso al grupo durante la proyección y comprobar el USR conocido del
tipo referido. Preservar validación de estado y duplicados. Archivos:
CppSemanticFunction.cpp, CppClangSessionTests.cpp y registros Markdown.
Prueba y validación agrupadas con la tanda.

## USR de tipos en aliases

T08-002/T08-005: conservar el USR del record o enum subyacente en la
representación de aliases y comprobarlo al resolver aliases simples, arreglos
y firmas cuando ambos lados sean conocidos. Mantener comprobaciones de nombre,
tipo y representación; la ausencia no crea una identidad ficticia. Archivos:
CppAliasEnumType.h, CppAliasRecordType.h, QualifiedTypeNames.cpp,
CppClangSessionTests.cpp y registros Markdown. Pruebas agrupadas con la tanda.

## Dependencias de record en firmas

T08-002/T08-003: conservar el USR acordado en cada grupo de records y comprobar
el USR del tipo de firma al seleccionar la dependencia; un desacuerdo conocido
no debe seleccionar otro record por nombre. Preservar la resolución anterior
si alguno falta y los controles de estado, layout y comparación estructural.
Archivos: CppSemanticRecord.h/.cpp, CppClangSessionTests.cpp y registros
Markdown. La prueba se ejecutará con la validación agrupada de la tanda.

## Correspondencia de USR en referencias de record

T08-002/T08-010: al proyectar un record C/C++ referido desde funciones,
globales o campos, comprobar que su USR conocido coincide con el tipo
instalado; mantener la ruta existente cuando falte información y preservar
todos los controles de layout. Archivos: CppSemanticFunction.cpp,
CppSemanticInventory.cpp, CppSemanticRecord.cpp, CppClangSessionTests.cpp y
registros Markdown. Validar el conjunto con la tanda.

## Identidad de records agrupados

T08-002/T08-010/T08-011: al agrupar records de un mismo nombre y proyecto,
tratar USR conocidos distintos como conflicto y transportar un USR conocido al
tipo importado cuando el grupo no esté en conflicto. Preservar los controles
de layout, ODR y comparación estructural y el comportamiento cuando Clang no
da USR. Archivos: CppSemanticRecord.cpp, CppClangSessionTests.cpp y registros
Markdown. Pruebas y validación agrupadas con la tanda.

## Identidad de enum al resolver firmas C

T08-002/T08-009: el proveedor y el vínculo de firmas C seleccionan enums por
nombre y representación; ahora que Clang aporta USR, conservarlo en el tipo
importado y evitar asociar una firma con otro USR conocido. Conservar las
comprobaciones de representación y el comportamiento cuando falta algún USR.
Archivos: ProjectNameLookup.h, CppSemanticInventory.cpp, CppSemanticEnumType.h,
CppClangSessionTests.cpp y registros Markdown. No presentar USR igual como
prueba de ABI. Las pruebas se agrupan con la tanda.

## Inicializador de prueba de identidad referida

La compilación de `92d212d3e` advirtió que el fixture sintético de
CppRecordTypeIdentity omite el nuevo campo opcional clangUsr. Inicializarlo
explícitamente como ausente en CppClangSessionTests.cpp; no cambiar semántica.
Verificar con la próxima compilación agrupada.

## Símbolos contradictorios de una misma función Clang

T08-002/T08-003: el agrupador usa símbolo de enlace y nombre; dos snapshots con
el mismo USR y propietario pero símbolos externos distintos pueden presentarse
como candidatos independientes. Detectar la contradicción antes de agrupar,
sin fusionar USR ni omitir verificaciones ABI y sin rechazar alias con USR
distintos que compartan símbolo. Presupuesto: CppSemanticFunctionGroups.cpp,
CppClangSessionTests.cpp y registros Markdown. La validación se agrupa por tanda.
La selección por demanda también debe incluir las entradas del mismo USR y
conservar sus orígenes para reportar conflictos diferidos.

## Identidad de tipos referidos por declaraciones C

T08-002: conservar en CppType el USR de la declaración canónica de cada record
o enum descrito por Clang, también cuando aparece bajo punteros y funciones.
Comparar en pruebas ese dato con el USR de su snapshot de declaración. No
reemplazar aún las comprobaciones de layout ni declarar equivalencia ABI por
coincidencia de USR. Archivos: CppType.h, ClangSession.cpp,
CppClangSessionTests.cpp y registros Markdown. Validación agrupada por tanda;
NSLink y fuentes LLVM permanecen protegidos.

## Identificadores de declaraciones de Clang

T08-002: capturar opcionalmente el USR que proporciona Clang 23.1.0 para las
declaraciones de funciones, globales, enums, enumeradores, aliases, records,
campos, métodos, campos estáticos y namespaces. Un USR no demuestra ABI ni
compatibilidad entre declaraciones; si Clang no lo genera se conserva ausencia.
Presupuesto: estructuras de snapshots `include/nativesharp/Interop/Cpp*Declaration.h`,
CppGlobalDeclaration.h, ClangSession.cpp, CMakeLists.txt,
CppClangSessionTests.cpp y registros Markdown. No cambiar aún selección ni
semántica. Pruebas de estabilidad y diferenciación agrupadas con la tanda;
NSLink y fuentes LLVM permanecen protegidos.

## Almacenamiento de puntero global importado volatile

T08-015: para un global C/C++ importado cuyo propio puntero es volatile, de 64 bits
y con modo MS y alineación acreditados por Clang, preservar acquire/release en
su lectura y escritura directas. No transferir volatile al objeto apuntado ni
alterar campos, punteros indirectos o destinos no acreditados. Archivos:
HirNameExpressionLowering.cpp, OopLlvmSupport.h,
OopLlvmExpressionLeaves.cpp, OopLlvmArraysAndStores.cpp, Api.h, Api.c, Main.ns,
validar.cmd y verificar_ir_c.py del fixture records C y registros Markdown.
Verificar IR de C y NativeSharp y ejecución agrupada O0–O3. NSLink y LLVM son
de solo lectura.

## Correcciones de la prueba de identidad

La batería en cb60b585e falló al enlazar `nsc_namespace_function_llvm_tests`:
`nsc_frontend` referencia `PrecompiledSemanticSnapshotCodec::decode`, definido
en `nsc_precompiled`. Añadir ese enlace al objetivo. La compilación también
advirtió una vida temporal al obtener el primer cuerpo mediante un span
temporal: conservar el span en una variable local. Archivos: CMakeLists.txt,
NamespaceFunctionLlvmTests.cpp y registros Markdown. Repetir la batería.

## Precisión de la comprobación IR de enteros volatile

La prueba previa comprueba acquire/release pero acepta cualquier ancho y no
exige el valor de alineación. Hacer que los seis globales comprueben su ancho y
alineación concretos (8/1, 16/2, 64/8). Archivos: validar.cmd del fixture de
records C y registro Markdown. Ejecutar con la siguiente batería agrupada.

## Identidad de caché de código para volatile importado

T08-015: `writeNodeFlags` no incorpora la opción MS volatile ni la evidencia de
alineación, aunque `isClangMsVolatileIntegerGlobal` cambia el IR según ambas.
Una identidad igual puede reutilizar código con orden distinto. Añadir ambos
opcionales a la identidad de definición y versionarla; probar los tres estados
de la opción y ausencia/presencia de alineación sobre un mismo MIR. Archivos:
ProjectOopMirCacheIdentity.cpp, NamespaceFunctionLlvmTests.cpp,
validar-tanda-08.cmd y registros Markdown. La verificación se agrupa con la
tanda; NSLink y el árbol LLVM son de solo lectura.

## Globales volatile enteros de 8, 16 y 64 bits

T08-015: extender la ruta de globales directos importados a enteros con y sin
signo de 8, 16 y 64 bits cuando Clang confirme tamaño, alineación suficiente y
semántica MS volatile. Preservar las demás formas y tipos. Presupuesto:
HirNameExpressionLowering.cpp, OopLlvmSupport.h, OopLlvmExpressionLeaves.cpp,
OopLlvmArraysAndStores.cpp, Api.h, Api.c, Main.ns y validar.cmd del fixture de
records C y registros Markdown. Comprobar IR acquire/release y ejecución con C
en la validación agrupada. NSLink y fuentes LLVM permanecen protegidos.

## Evidencia de alineación atómica en MIR

Para T08-015, conservar en MIR la alineación atómica opcional ya calculada en
HIR: `alignment == 1` por sí solo no distingue un byte comprobado de un dato
ausente. Archivos autorizados: ProjectOopMirNode.h, MirNodeMachine.cpp,
MirCache.cpp, ProjectOopLoweringTests.cpp y registros Markdown. Preservar la
emisión existente y versionar la identidad de caché. Preparar la prueba de
restauración; ejecutarla con la validación agrupada de la tanda. NSLink y LLVM
permanecen protegidos.

## Enlace de prueba de lowering MIR

El enlace de nsc_project_oop_lowering_tests falló por el símbolo
PrecompiledSemanticSnapshotCodec::decode, usado por nsc_frontend y definido en
nsc_precompiled. Añadir esa biblioteca a ese objetivo de prueba. Archivos:
CMakeLists.txt y documentación. Repetir enlace y validación agrupada.

## Rutas de ejecutables de prueba con NSC_BUILD_TESTS

Con `NSC_BUILD_TESTS=ON`, CMake genera las tres pruebas de sintaxis y MIR bajo
`tests`, no `bin`. Ajustar el script para evitar ejecutar binarios anteriores.
Archivos: validar-tanda-08.cmd y documentación. Repetir la batería agrupada.

## Código de error del build incremental

La batería en d3dc414a8 continuó tras un enlace fallido con código -1 porque
`if errorlevel 1` no detectó ese valor negativo. Normalizar todo código no cero
del helper a 1. Archivos: construir-frontend-tanda1.cmd y documentación.
Repetir la validación tras corregir el enlace y las rutas.

## Configuración del objetivo de prueba MIR

La batería en c552a69b9 se detuvo porque `NSC_BUILD_TESTS` estaba desactivado y
no existía el objetivo de restauración MIR. Habilitarlo en la configuración ON
existente antes de construir los objetivos nombrados, sin compilar todos los
tests. Archivos: validar-tanda-08.cmd y documentación. Repetir la batería.

## Prueba de orden MS en global directo

Exigir IR acquire/release del global volatile de 32 bits y comprobar desde C la
asignación simple y compuesta. Archivos: Main.ns, validar.cmd del fixture de
records C y documentación. Ejecutar con la validación agrupada.

## Orden MS para global volatile directo de 32 bits

Emitir acquire en lectura y release en escritura, incluida asignación compuesta,
solo para un global importado int/uint volatile con opción MS activa y prueba
de alineación de cuatro bytes. Mantener los demás accesos sin cambios. Archivos:
OopLlvmSupport.h, OopLlvmExpressionLeaves.cpp, OopLlvmArraysAndStores.cpp y
documentación. Preparar comprobaciones de IR y ejecutar validación agrupada.

## Alineación probada de global volatile de 32 bits

Preparar en HIR y MIR una alineación mínima de cuatro bytes solo para globales
externos volatile int/uint de 32 bits con opción MS activa y alineación conocida
de al menos 32 bits. Conservarla en identidad y caché; no emitir todavía
atomicidad ni inferir soporte para otros tamaños. Archivos: ProjectOopHirNode.h,
HirNameExpressionLowering.cpp, MirNodeMachine.cpp, HirCacheIdentity.cpp,
MirCache.cpp y documentación. Validación agrupada pendiente.

## Prueba de tamaño y alineación en símbolo global

Ampliar el caso del proveedor para exigir tamaño y alineación conocidos en el
símbolo C++ importado. Archivos: CppClangSessionTests.cpp y documentación.
Ejecutar con la validación agrupada de la tanda.

## Tamaño y alineación en símbolo global importado

Transportar tamaño y alineación conocidos al símbolo externo seleccionado,
manteniendo ausencia cuando Clang no aporta el dato. Archivos:
NamespaceStateSymbol.h, CppSemanticInventory.cpp y documentación. No cambiar
todavía el acceso ni la emisión. Preparar comprobación y validar por tanda.

## Prueba de tamaño y alineación de global seleccionado

Preparar controles de tamaño, mínimo de alineaciones y alineación desconocida
para declaraciones repetidas. Archivos: CppClangSessionTests.cpp y documentación.
Ejecutar con la siguiente validación agrupada.

## Tamaño y alineación al seleccionar un global

Conservar el tamaño capturado de Clang y una cota conservadora de alineación al
seleccionar un global: mínimo entre declaraciones conocidas y ausencia si
alguna es desconocida. No decidir aún elegibilidad atómica ni cambiar el IR.
Archivos: CppSemanticInventory.h/.cpp y documentación; preparar pruebas y
validar con la tanda.

## Prueba de alineación de globales Clang

Preparar un global con alineación explícita de 16 bytes y un arreglo incompleto,
comprobando el valor y la ausencia respectivamente. Archivos:
CppClangSessionTests.cpp y documentación. Ejecutar con la batería agrupada.

## Alineación declarada de global C/C++

Capturar desde ASTContext la alineación efectiva de cada declaración global de
tipo completo, sin inventarla para tipos incompletos. Archivos:
CppGlobalDeclaration.h, ClangSession.cpp y documentación. No alterar todavía
selección ni emisión; preparar pruebas y validar con la tanda.

## Prueba de identidad HIR de opción volatile

Preparar una comprobación de que los tres estados de la opción producen
identidades HIR diferentes. Archivos: ProjectOopLoweringTests.cpp y
documentación. Se ejecutará con la modalidad enfocada ya añadida a la tanda.

## Prueba de restauración MIR de opción volatile

Ampliar la prueba existente de restauración MIR para alternar los tres estados
de la opción y añadir su ejecución enfocada a la batería de la tanda. Archivos:
ProjectOopLoweringTests.cpp, validar-tanda-08.cmd y documentación. La prueba
utiliza el TEMP de A configurado por el script y aún no se ha ejecutado.

## Transporte inicial de opción volatile a HIR y MIR

Transportar el valor de sesión, incluido desconocido, desde un global externo
volatile a su nodo HIR y MIR y conservarlo en las identidades y caché MIR. No
emitir órdenes atómicos ni extenderlo aún a campos o punteros. Archivos: nodos
HIR/MIR, HirNameExpressionLowering.cpp, MirNodeMachine.cpp, HirCacheIdentity.cpp,
MirCache.cpp y documentación. Preparar pruebas y validar en conjunto.

## Prueba de opción volatile en símbolo global

Preparar comprobación de que el símbolo producido conserva la opción de su
sesión real. Archivos: CppClangSessionTests.cpp y documentación. Ejecutar con
la siguiente validación agrupada.

## Opción volatile en símbolo de global importado

Conservar en NamespaceStateSymbol el valor conocido o desconocido del candidato
de global C/C++ seleccionado. Los globales NativeSharp y la emisión de código
permanecen sin cambios. Archivos: NamespaceStateSymbol.h,
CppSemanticInventory.cpp y documentación. Preparar prueba y validar en tanda.

## Prueba de selección de opción volatile

Preparar controles de acuerdo, desacuerdo y opción desconocida al seleccionar
declaraciones repetidas de un global. Archivos: CppClangSessionTests.cpp y
documentación. Ejecutar con la batería agrupada.

## Opción volatile al seleccionar un global

Conservar la opción efectiva en el candidato seleccionado solo si todas sus
declaraciones y aliases coinciden y el valor está disponible; preservar ausencia
ante discrepancia o desconocimiento. No alterar todavía emisión ni diagnósticos.
Archivos: CppSemanticInventory.h/.cpp y documentación. Preparar pruebas y
validar el conjunto de la tanda.

## Prueba de procedencia de volatile en globales

Preparar comprobación de que los globales C y C++ conservan el valor de su
propia sesión en el inventario. Archivos: CppClangSessionTests.cpp y
documentación. La validación se agrupa con la tanda.

## Procedencia de volatile en inventario de globales

Conservar en cada entrada del inventario de globales la opción efectiva de su
sesión, incluida la ausencia cuando no está disponible. No cambiar selección ni
emisión todavía. Archivos: CppSemanticInventory.h/.cpp y documentación; preparar
prueba y validar junto con la tanda.

## Prueba de consulta de volatile efectivo

Preparar controles de disponibilidad del valor de Clang en sesión activa y
ausencia tras cierre, sin suponer el valor por el triple. Archivos:
CppClangSessionTests.cpp y documentación. Ejecutar con la tanda agrupada.

## Consulta de volatile efectivo de Clang

Exponer mediante la sesión el valor real de LangOpts.MSVolatile, con nullopt si
no está disponible o la sesión cerró. No inferirlo del triple ni modificar aún
la emisión de NativeSharp. Archivos: CppInteropSession.h, ClangSession.cpp y
documentación. Preparar pruebas y ejecutar con la batería agrupada de la tanda.

## Alias C a enum const volatile

Preparar lectura mediante alias a enum const volatile y rechazo de escritura.
Archivos: fixture enum-funcion, su script y documentación. La ejecución se
reserva para la siguiente validación agrupada de la tanda.

## Alias C a struct const volatile

Preparar lectura de un campo mediante alias a struct const volatile y rechazo
de escritura. Archivos: fixture de records C, su script y documentación. La
ejecución se reserva para la siguiente validación agrupada de la tanda.

## Ubicación válida del global en el fixture

La validación agrupada de 5e89bf69b alcanzó el proyecto de records y NSC rechazó
una variable declarada fuera de todo namespace. Mover solo el global del caso
positivo y negativo a un namespace y actualizar sus referencias. Archivos:
Main.ns, VolatileAliasGlobalAddress.ns.txt y documentación. Repetir la prueba.

## Verificador de IR C con volatile atómico

La batería de la revisión 7c9a765b6 falló en bump_volatile: Clang emitió
`load atomic volatile i32` y `store atomic volatile i32`, que el verificador
de texto rechazaba. Aceptar también esa forma manteniendo la exigencia de
volatile dentro de cada función. Archivos: verificar_ir_c.py y documentación.
No cambiar Clang ni la semántica del programa. Repetir la validación agrupada.

## Rechazo de pérdida volatile en alias a enum

Preparar el caso negativo de pasar un alias a enum volatile a una función
NativeSharp que espera puntero ordinario. Archivos: fixture enum-funcion, su
script y documentación. El caso espera validación agrupada.

## Rechazo de pérdida volatile en alias a struct

Preparar el caso negativo de pasar un alias a struct volatile a una función C
que espera puntero ordinario. Archivos: fixture de records C, su script y
documentación. El caso espera validación agrupada.

## Prueba de alias a enum volatile

Preparar un alias C de puntero a enum volatile y usarlo para escribir y leer
el enum compartido con C. Archivos: Api.h, Main.ns del fixture enum-funcion y
documentación. La validación agrupada sigue pendiente.

## Prueba de alias a struct volatile

Preparar un alias C de puntero a struct volatile y comprobar escritura de campo
observada por C y por IR. Archivos: fixture de records C, script y documentación.
La prueba espera la validación agrupada.

## Aliases a records y enums volatile

Preparar la proyección de aliases de un puntero a record o enum volatile ya
importado; conservar identidad y const y rechazar niveles externos aún no
representados. No mutar el tipo de Clang. Archivos: QualifiedTypeNames.cpp y
documentación; las pruebas y validación agrupada siguen pendientes.

## Prueba de campo NativeSharp con alias volatile

Preparar un campo NativeSharp tipado con el alias C: comprobar escritura al
apuntado en IR y memoria C, y rechazo de conversión del campo a puntero sin
volatile. Archivos: fixture de records C, su script y documentación. Espera la
validación agrupada de la tanda.

## Prueba de retorno NativeSharp const volatile

Preparar lectura del resultado NativeSharp declarado con alias C const volatile
y rechazo de escritura al apuntado. Archivos: fixture de records C, su script
y documentación. Ambos casos esperan la validación agrupada de la tanda.

## Const del retorno de función NativeSharp con alias C

Conservar el const del apuntado del tipo declarado cuando se adapta una función
NativeSharp; preservar la vía de resultados C previamente existente. Archivo:
MethodBodyBuildCoordinator.cpp y documentación. La validación agrupada sigue
pendiente y cubrirá también el alias combinado const volatile.

## Control de alias volatile con indirección adicional

Preparar un caso que exija diagnóstico para el uso del alias seguido de otro
puntero mientras ese nivel no conserve su calificador. Archivos: fixture de
records C, script y documentación. Es una protección provisional; los niveles
anidados siguen dentro del objetivo y la prueba espera validación agrupada.

## Prueba de estado global con alias volatile

Preparar un estado global NS tipado con el alias C: asignar el puntero,
escribir al apuntado y comprobar la operación volatile en IR y su resultado C.
Preparar el rechazo de su dirección mientras no se representen los niveles
anidados. Archivos: fixture de records C, su script y documentación. Ejecutar
solo junto con la validación agrupada de la tanda.

## Alias C a apuntado const volatile

Preparar lectura del alias combinado y comprobación de rechazo de escritura.
Archivos: Api.h, Main.ns, ConstVolatileAliasWrite.ns.txt y validar.cmd del
fixture de records C, y documentación. Validación agrupada aún pendiente.

## Conversión explícita al alias volatile

Preparar un caso positivo que convierte al alias C, escribe por el resultado y
comprueba tanto el valor observado desde C como la instrucción LLVM volatile.
Archivos: Main.ns y validar.cmd del fixture de records C y documentación.
La ejecución corresponde a la validación agrupada de la tanda 08.

## Comprobaciones del alias C a puntero volatile

Preparar comprobación de IR de la escritura por el alias y rechazos de pérdida
del calificador en inicialización y retorno. Archivos: fixture
pruebas-tanda-08-record-c, su script y documentación. La ejecución se agrupa
con la validación de la tanda; no se afirma que las pruebas pasen todavía.

## Proyección de alias C a puntero escalar volatile

Proyectar un alias de puntero de un nivel a escalar volatile como TypeSyntax,
conservando const y volatile, sin mutar el tipo capturado por Clang. Mantener
rechazados usos con modificadores externos y formas aún no representadas.
Archivos: QualifiedTypeNames.cpp y documentación. El caso preparado y las
regresiones se ejecutarán con la validación agrupada de la tanda 08.

## Caso de alias C de puntero a volatile

Preparar el proyecto mixto existente para comprobar que un typedef C de puntero
a volatile se conserva en parámetro, retorno y variable local de NativeSharp,
y que las lecturas y escrituras alcanzan la misma memoria C. Archivos: Api.h,
Main.ns de pruebas-tanda-08-record-c y documentación. El alias todavía no está
habilitado y el caso se ejecutará con la validación agrupada de la tanda.

## Apuntado volatile en estados globales NativeSharp

Preparar la lectura del calificador desde el tipo declarado de un estado global
NativeSharp, conservando el dato de globales C y rechazando su dirección cuando
el puntero anidado no puede representarlo. Archivos: IdentifierAnalyzer.cpp y
documentación. Los aliases permanecen desactivados y falta validación agrupada.

## Conversión explícita con apuntado volatile declarado

Conservar el calificador en el resultado cuando el tipo explícito lo declara;
seguir rechazando su pérdida hacia punteros sin volatile o enteros del tamaño
de una dirección. Archivos: ConversionExpressions.cpp y documentación. La
activación de aliases y la validación agrupada de la tanda siguen pendientes.

## Retorno con apuntado volatile declarado

Permitir que un retorno de puntero conserve volatile cuando la declaración del
método ya lleva ese calificador, sin permitir que desaparezca al retornar un
puntero no calificado. Archivos: StatementAnalyzer.cpp y documentación. La
activación de aliases y la validación agrupada de la tanda siguen pendientes.

## Contrato de apuntado volatile en campos

Preparar la propagación desde campos y sus respaldos automáticos, permitir
inicializadores cuyo destino ya conserva volatile y mantener protegida la
dirección del campo frente a pérdida en un nivel anidado. Archivos:
IdentifierAnalyzer.cpp, MemberAccessAnalyzer.cpp, MethodBodyBuildCoordinator.cpp
y documentación. Los aliases todavía no activan el dato; validar por tanda.

## Contrato de apuntado volatile en locales tipados

Preparar que un local con tipo cuya sintaxis conserva volatile mantenga ese
contrato y acepte un inicializador compatible; seguir rechazando su pérdida
hacia un tipo sin calificador. Archivos: ProjectMethodBodyModel.h,
StatementAnalyzer.cpp y documentación. La activación de aliases y la
validación agrupada siguen pendientes.

## Contrato de apuntado volatile en declaraciones tipadas

Preparar lectura del dato de TypeSyntax para parámetros y miembros, y para el
adaptador de funciones. Archivos: ProjectNamespaceFunctionModel.cpp,
BuildAndDeclaration.cpp, MethodBodyBuildCoordinator.cpp y documentación.
Todavía no se habilita ningún typedef; los valores ordinarios siguen falsos.

## Identidad y volcado del apuntado volatile en sintaxis

Conservar el calificador en el patrón genérico y hacerlo visible en el volcado
de TypeSyntax, sin modificar la identidad ABI de punteros. Archivos:
GenericPatterns.cpp, AstDumper.cpp y documentación. La proyección de typedefs
sigue deshabilitada y la verificación se agrupa con la tanda.

## Pruebas de clonación y persistencia del apuntado volatile

Ampliar los casos existentes de clonación de TypeSyntax y serialización del
árbol para exigir conservación del dato volatile independiente de const y
restrict. Archivos: PrecompiledSyntaxNodeIndexTests.cpp,
ParserConstructionCloneFinalModule.cpp y documentación. Preparar para la
validación agrupada, sin afirmar resultado antes de ejecutarla.

## Persistencia de sintaxis de apuntado volatile

Conservar el nuevo dato de TypeSyntax en la escritura y lectura de árboles de
tipos. Incrementar los esquemas de caché genérica y unidad sintáctica que
reutilizan el formato para no interpretar bytes antiguos con el diseño nuevo.
Archivos: TemplateCatalogCodec.cpp, SyntaxProductCacheCodec.cpp y documentación.
La ruta de alias todavía no se activa; verificar restauración en la tanda.

## Base de sintaxis para typedefs con apuntado volatile

Añadir el dato de volatile inmediato a TypeSyntax y conservarlo en las rutas
que clonan ese objeto. Archivos: TypeSyntax.h/.cpp, Parser.cpp,
ProjectOpaqueResultModel.cpp, TypeAliasExpander.cpp, GenericTypeInference.cpp,
GenericCloner.cpp y documentación. El dato permanece falso hasta conectar
aliases y caché; no se admite sintaxis nueva ni se declara el soporte cerrado.
La serialización, identidad y consumidores se preparan antes de activarlo.

## Dirección de parámetro con apuntado volatile

Preparar la misma protección aplicada a locales para parámetros por valor con
apuntado volatile: no formar un doble puntero que pierda el calificador.
Archivo: IdentifierAnalyzer.cpp y documentación. La protección es provisional
y no modifica la asignabilidad ni los parámetros ordinarios.

## Resultados C puntero a struct o enum volatile

Extender la proyección de resultados C de un nivel a struct o enum importado,
con identidad y representación resueltas, preservando el tipo original.
Archivos: CppSemanticFunction.cpp, fixtures pruebas-tanda-08-record-c y
pruebas-tanda-08-enum-funcion, tracking y documentación. Preparar accesos a
campo y elemento; no habilitar retornos anidados ni funciones C++.

## Llamada C con puntero a enum volatile importado

Preparar el paso del global importado a parámetros C volatile y const volatile
sin perder identidad de enum. Archivos: fixture
pruebas-tanda-08-enum-funcion y documentación. Comprobar en la validación
agrupada; no extrapolar a callbacks ni tipos anidados.

## Global C puntero a enum volatile

Preparar la proyección de un global C que apunta directamente a un enum
importado cuya representación coincide con Clang. Archivos:
CppSemanticInventory.cpp, fixture pruebas-tanda-08-enum-funcion y documentos.
No habilitar tipos anidados sin representación; probar lectura, escritura e IR
en la validación agrupada.

## Struct volatile apuntado con campo puntero

Preparar una prueba que distinga el acceso volatile al campo puntero del acceso
ordinario al objeto que ese campo apunta, al llegar al struct por otro puntero.
Archivos: fixture pruebas-tanda-08-record-c y documentación. Ejecutar con la
validación agrupada y no afirmar cobertura total de agregados.

## Global C puntero a struct volatile

Preparar la proyección de un global C que apunta directamente a un struct
importado con layout comprobado. Conservar volatile hasta el acceso de campo;
no habilitar punteros anidados. Archivos: CppSemanticInventory.cpp, fixture
pruebas-tanda-08-record-c y documentación. Preparar lectura, escritura y
rechazo de tomar una dirección no calificada; la validación agrupada y el
comportamiento de agregados completos siguen pendientes.

## HIR de campos alcanzados por puntero a struct volatile

Propagar volatile al campo cuando el receptor es una dirección obtenida por
desreferencia y esa dirección ya está calificada. Archivo:
HirCompletionValueFrames.cpp y documentación. Una carga volatile del propio
puntero no califica el objeto; preservar esa separación. La importación de
punteros C a struct volatile sigue pendiente hasta preparar sus pruebas.

## Dirección de un local con apuntado volatile

Impedir provisionalmente obtener la dirección de un local cuyo tipo inferido
contiene volatile en el apuntado, ya que T** aún no conserva ese calificador
interior. Archivo: IdentifierAnalyzer.cpp, fixture pruebas-tanda-08-record-c y
documentación. No modificar la asignabilidad del local ni la dirección de
punteros ordinarios. Preparar diagnóstico y prueba para la tanda agrupada.

## Puntero C const a escalar volatile

Preparar comprobación de que const del puntero impide reasignarlo, pero no
impide escribir su apuntado volatile. Archivos: fixture
pruebas-tanda-08-record-c y documentación. Ejecutar en la validación agrupada;
no extrapolar a todos los niveles de indirección.

## Proyección inicial de globales con modo C

Conservar el modo C de la sesión al proyectar el inventario inicial de
globales, también cuando el proveedor no recibe ProjectNameLookup. Archivos:
CppSemanticInventory.cpp, CppClangSessionTests.cpp y documentación. Preparar
comprobación del global puntero volatile; validar junto con la tanda.

## Pruebas de escapes de puntero C volatile

Preparar controles del proyecto mixto para comprobar que una tupla o una
conversión explícita no pierden el calificador del apuntado. Archivos:
fixture pruebas-tanda-08-record-c y documentación. Son barreras provisionales
y no sustituyen la representación completa de tipos calificados. Ejecución
con la validación agrupada.

## Control de límites de resultados y parámetros C++

Preparar controles de Clang que comprueben que la normalización de tipos de
enlace C no habilita por accidente firmas C++ con apuntado volatile sin su
resolución de sobrecargas. Archivos: CppClangSessionTests.cpp y documentación.
El soporte C++ permanece en el objetivo y requiere implementación posterior.

## Resultado C puntero a escalar volatile

Proyectar el resultado C de un nivel a escalar volatile preservando el tipo
original de Clang y transportando el calificador por el adaptador y el
resultado de llamada ya preparados. Archivos: CppSemanticFunction.cpp,
CppClangSessionTests.cpp, fixture pruebas-tanda-08-record-c y documentación.
No habilitar resultados C++ ni apuntados agregados o anidados. Probar
lectura, escritura y rechazo de pérdida de calificador en la tanda agrupada.

## Global C puntero a escalar const volatile

Preparar la prueba de lectura y de rechazo de escritura de un apuntado con
ambos calificadores, sin habilitar tipos adicionales. Archivos: fixture
pruebas-tanda-08-record-c y documentación; ejecución junto con la tanda.

## Global C puntero a escalar volatile

Preparar la importación de un global C cuyo apuntado inmediato es escalar
volatile, usando la propagación y protecciones ya preparadas. Archivos:
CppSemanticInventory.cpp, fixture pruebas-tanda-08-record-c y documentación.
No habilitar C++, punteros anidados, structs ni enums con este cambio. El
fixture debe ejercitar lectura, escritura, índice, local inferido y comprobar
IR y pérdida de calificador; ninguna capacidad se considera verificada hasta
la validación agrupada y la auditoría de escapes.

## Protección provisional de punteros volatile en inicializadores de arreglo

Evitar que un inicializador de arreglo nativo almacene un puntero cuyo
calificador volatile del apuntado no se representa en el elemento. Archivo
ConstructionAndLifetimeExpressions.cpp y documentación. El soporte de
elementos calificados permanece pendiente y dentro del alcance.

## Protección provisional de punteros volatile en tuplas

Las identidades actuales de tupla conservan sólo claves de tipo y no los
calificadores de sus elementos. Rechazar provisionalmente un elemento que
perdería volatile; archivo OperatorAndConditionalExpressions.cpp y
documentación. La importación permanece bloqueada y el soporte de tuplas
calificadas sigue dentro del objetivo.

## Preparación del contrato volatile de resultado

Conservar en el adaptador y en resultados de llamadas la calificación
inmediata observada por Clang; persistir el dato en el esquema de snapshot
vigente. Archivos: ClassMemberSymbol.h, ClassMemberSnapshotCodec.cpp,
MethodBodyBuildCoordinator.cpp, CallAnalyzer.cpp y documentación. La barrera
de proyección de resultados volatile permanece activa; no se afirma soporte
ni se cambia la sintaxis de NativeSharp.

## Prueba interna de move con apuntado volatile

Extender el fixture inyectado para comprobar que move de un local conserva
volatile hasta las cargas y escrituras. Archivos: CppClangSessionTests.cpp y
documentación. No habilitar la importación real; ejecución en la validación
agrupada.

## Propagación volatile en move

Conservar el calificador inmediato al construir el resultado de move para
locales, parámetros, campos y elementos. Archivo: MoveExpressions.cpp y
documentación. No habilitar la importación ni modificar ownership; falta
validación agrupada.

## Uso de parámetros volatile e inicialización de campos

Propagar el contrato de parámetro por valor a su expresión y rechazar
provisionalmente la inicialización de un campo cuyo tipo no conserva volatile.
Archivos: IdentifierAnalyzer.cpp, MethodBodyBuildCoordinator.cpp y
documentación. No se habilita sintaxis nueva ni se retira la barrera de
importación; faltan pruebas agrupadas.

## Límites de parámetros y resultados volatile anidados

Añadir controles al fixture de Clang para asegurar que el soporte preparado de
parámetros inmediatos no habilita por accidente retornos volatile ni punteros
anidados calificados. Archivos: CppClangSessionTests.cpp y documentación. Los
rechazos son protecciones temporales, no el alcance final.

## Protección provisional de llamadas indirectas y variádicas

Mientras las firmas estructurales no representen volatile, rechazar argumentos
con apuntado volatile en llamadas indirectas y en los argumentos variádicos
sin contrato tipado. Archivo: CallAnalyzer.cpp y documentación. Es una
restricción provisional; el objetivo conserva el soporte de estas llamadas.
La importación real permanece cerrada.

## Prueba aislada de contrato volatile en llamadas

Inyectar un global puntero con apuntado volatile y proyectar las funciones C
reales del fixture de Clang. Preparar caso aceptado cuando el parámetro
conserva volatile y rechazo cuando lo descarta. Archivos:
CppClangSessionTests.cpp y documentación; la importación real continúa cerrada
y la ejecución se agrupa al finalizar la tanda.

## Correspondencia volatile en argumentos de llamada

Permitir que un argumento con apuntado volatile alcance un parámetro que
conserva explícitamente ese contrato; seguir rechazando su pérdida y los
paquetes sin contrato equivalente. Archivos: CallAnalyzer.cpp,
ConstructorCandidateEvaluation.cpp y documentación. Mantener cerrada la
importación de globales hasta cubrir llamadas indirectas y variádicas.

## Contrato volatile de parámetros y snapshot

Conservar en MethodParameterSymbol el volatile inmediato de parámetros C
importados, junto con su serialización en snapshot. Versionar el esquema para
evitar interpretar el formato anterior con offsets nuevos; la carga de
snapshots incompatibles vuelve al análisis normal. Archivos:
MethodParameterSymbol.h, CppSemanticFunction.cpp, SnapshotCodecSupport.cpp,
PrecompiledSemanticSnapshotCodec.cpp, CppClangSessionTests.cpp y documentación.
La aceptación de argumentos volatile se implementa después; la validación
agrupada incluye restauración de snapshots.

## Prueba aislada del apuntado volatile en globales

Inyectar exclusivamente en CppClangSessionTests un global con tipo y símbolo
de Clang y marca de apuntado volatile. Comprobar semántica, IR de carga y
escritura, y rechazo de pérdida de calificador. No retirar la barrera real de
importación. Archivos: tests/unit/CppClangSessionTests.cpp y documentación.
Ejecutar con la validación agrupada incremental de la tanda.

## Protección provisional del retorno de punteros volatile

Rechazar retornos que descartarían el volatile inmediato mientras los tipos de
resultado no lo representan. Archivo: StatementAnalyzer.cpp y documentación.
No afecta retornos ordinarios; la importación sigue bloqueada hasta completar
los demás recorridos y validar de forma agrupada.

## Protección provisional de argumentos con apuntado volatile

Preparar el rechazo de argumentos volatile en las rutas ordinarias de resolución
de llamadas y constructores, hasta representar el calificador en cada
parámetro y su persistencia. Archivos: CallAnalyzer.cpp,
ConstructorCandidateEvaluation.cpp y documentación. Es una protección
temporal, no el comportamiento final; quedan por auditar llamadas indirectas,
variádicas y demás vías antes de habilitar el origen.

## Conversiones explícitas de punteros a volatile

Impedir provisionalmente que una conversión produzca un puntero o entero y
descarte el calificador mientras no exista sintaxis y semántica de destino
calificado. Archivo: ConversionExpressions.cpp y documentación. No cambiar la
conducta de punteros ordinarios; la importación sigue bloqueada.

## Asignaciones de punteros con apuntado volatile

Preparar el rechazo de asignaciones que perderían el volatile inmediato del
apuntado. Archivos: AssignmentAnalyzer.cpp y documentación; el flujo sigue
deshabilitado en la importación hasta conectar llamadas, conversiones, retornos
y pruebas. No cambiar el contrato de otras conversiones de punteros.

## Propagación del apuntado volatile en expresiones

Conservar el calificador inmediato en aritmética de punteros, coalescencia y
condicionales, combinando las ramas sin descartarlo. Archivos autorizados:
OperatorExpressionAnalyzer.cpp, OperatorAndConditionalExpressions.cpp y
documentación. La barrera de importación sigue activa; se probará en la
validación agrupada una vez conectadas las rutas restantes.

## Calificador volatile inmediato en variables locales inferidas

Preparar la conservación del dato en locales inferidos a partir de una
expresión; rechazar inicialización explícita que lo descartaría mientras no
exista sintaxis de puntero calificado. Archivos: MethodLocalSymbol.h,
ProjectMethodBodyModel.h, StatementAnalyzer.cpp, IdentifierAnalyzer.cpp y
documentación. La importación continúa bloqueada; asignaciones, conversiones,
llamadas y validación quedan pendientes antes de habilitarla.

## Preparación HIR de accesos al objeto volatile apuntado

Usar el dato semántico inmediato para marcar la carga por desreferencia y la
dirección de destino de escrituras e índices; no confundirlo con el volatile
del almacenamiento del puntero. Archivos: HirCompletionTerminalFrames.cpp,
HirCompletionAccessFrames.cpp y documentación. No retirar la barrera de
importación hasta completar la propagación y las pruebas.

## Preparación de accesos al objeto volatile apuntado

Propagar al resultado semántico de desreferencia e índice el volatile inmediato
del apuntado; preservar la restricción de obtener direcciones sin tipo
calificado. Archivos: IdentifierAnalyzer.cpp, IndexAndRangeExpressions.cpp y
documentación. La importación y las conversiones siguen bloqueadas; el campo
preparado se mantiene inactivo para código ordinario. HIR y pruebas se
conectan en cambios posteriores antes de habilitar declaraciones.

## Calificador volatile del apuntado en globales importados

Preparar el transporte desde CppSemanticGlobal hasta NamespaceStateSymbol y el
resultado semántico. Añadir sólo el dato inmediato; no retirar la barrera que
rechaza punteros a volatile hasta conectar conversiones y generación.
Archivos: CppSemanticInventory.h/.cpp, NamespaceStateSymbol.h,
IdentifierAnalyzer.cpp y documentación. Mantener la toma de dirección
restringida; no modificar snapshots sin una ruta demostrada ni cerrar el ítem.

## Calificador volatile del objeto inmediatamente apuntado: base semántica

Preparar un indicador separado del volatile del almacenamiento del puntero,
primero en ExpressionResult, MethodExpressionTypeBinding y su registrador.
El indicador se inicia falso y no habilita todavía ninguna declaración C
rechazada. No se cambia el parser ni se presume soporte para varios niveles.
Archivos autorizados: esas dos cabeceras, MethodBodyBindingRecorder.cpp y la
documentación de la tanda; LLVM y NSLink continúan protegidos. La propagación,
los accesos y la validación agrupada son trabajo pendiente de esta tanda.

## Parámetro C puntero a void volatile

Preparar una llamada a función con enlace C que recibe volatile void* y
preservar el tipo original y const del apuntado. Archivos: proyección
CppSemanticFunction.cpp, fixture pruebas-tanda-08-record-c y documentación.
El fixture convierte el puntero dentro de C y comprueba la lectura; las
restricciones generales y la validación agrupada siguen vigentes.

## Parámetros C puntero a enum volatile

Extender la proyección de parámetros con enlace C a un enum importado y
resuelto por el inventario. Archivos: CppSemanticFunction.cpp, fixture
pruebas-tanda-08-enum-funcion y documentación de la tanda. Preservar el tipo
original, const, los límites anteriores y todas las rutas protegidas.
Verificar llamada y resultado con la batería agrupada incremental.

## Parámetros C puntero a struct volatile

Extender la misma entrada de enlace C a un struct importado y reconocido por
el inventario; mantener const y el tipo original. Archivos autorizados:
CppSemanticFunction.cpp, CppClangSessionTests.cpp, el fixture
pruebas-tanda-08-record-c y esta documentación. No habilitar el consumo de
punteros volatile desde NS, resultados, callbacks ni punteros anidados.
Verificar en la validación agrupada la llamada real y la conservación de const.

## Parámetros C puntero a escalar volatile

Se proyectan parámetros de enlace C de un nivel a escalares volatile para
llamadas desde NS; el calificador sigue en el tipo original de Clang y la
función C ejecuta los accesos. Se preserva const del apuntado. Archivos:
CppSemanticFunction.cpp, CppClangSessionTests.cpp, el fixture
pruebas-tanda-08-record-c y esta documentación. No se habilitan retornos,
globales, callbacks, punteros anidados ni sobrecargas C++ con este cambio.
La ejecución mixta y el rechazo de pérdida de const se comprueban en la
validación agrupada incremental; los ítems generales siguen abiertos.

## Tanda 08: declaraciones y tipos C

Se trabaja exclusivamente en NSC: interfaces y extracción de Clang, inventario,
representación de tipos y nombres, análisis semántico, pruebas y documentación
`nativesharp-interop`. Los temporales y productos van en A:. LLVM/Clang se
consulta como referencia de solo lectura; no se modifica NSLink ni se despliega
en C:. Se revisan todos los ítems, se implementan en conjunto y se hacen commits
por cambio e ítem; la construcción incremental y las pruebas se agrupan al final.
No se confunde información extraída del AST con un tipo ya usable desde NSC.

## Tanda 07: referencias y dependencias C/C++

Se trabaja en `A:/nsc-devel`: carga y modelo de proyectos, sesiones y procedencia
de declaraciones, controlador, pruebas y documentación `nativesharp-interop`.
Los productos y temporales se mantienen en A:. LLVM/Clang permanece de solo
lectura, NSLink no se modifica y no hay despliegue en C:. Primero se audita el
contrato existente y se implementa la tanda en conjunto; cada cambio e ítem se
registra por separado y la validación incremental se agrupa al final. Las
limitaciones de caché y de formatos de importación se mantienen explícitas hasta
contar con evidencia que permita retirarlas.

## Tanda 06: validación de sesiones

La escritura se limita a NSC: configuración CMake, pruebas de sesión y sus
auxiliares, y documentación `nativesharp-interop`. Los temporales y binarios de
prueba permanecen en A:. LLVM/Clang es referencia de solo lectura; no modificar
NSLink ni desplegar en C:. Se agrupan los ítems y la validación incremental al
final; cada cambio y cada ítem se registran en commits separados. Las pruebas
de versión y de inicio fallido deben comprobar el caso real, no uno parecido.

## Tanda 05: integración y diagnósticos

La tanda se realiza exclusivamente en `A:/nsc-devel`: frontend, controlador,
interfaces C++ de NSC, sus pruebas, configuración de compilación y esta
documentación. LLVM y Clang son referencias de solo lectura; NSLink permanece
sin cambios y no hay despliegue en C:. Cada cambio tendrá su propio commit; la
construcción incremental y la validación se agrupan al terminar la implementación.
Las ubicaciones de Clang se preservarán sin inventar correspondencias cuando no
exista una fuente de NSC que permita resolverlas de forma segura.

**Actualización del 5 de noviembre de 2026:** la ubicación vigente de este
trabajo, sus pruebas y su documentación es `A:/nsc-devel/nativesharp-interop`.
El código fuente de `A:/llvm-project-al82dev` se consulta como referencia de
solo lectura. Las rutas anteriores de este registro son históricas; para el
traslado y su historial, ver [TRASLADO-A-NSC.md](TRASLADO-A-NSC.md).

## Investigación incremental: objetos C++ locales por valor (2026-11-05)

- Objetivo: determinar el camino completo para construir un local C++ en su ubicación definitiva y destruirlo exactamente una vez. Alcance de esta investigación: lectura de NSC y esta bitácora; no retirar barreras ni modificar el compilador. Revisión NSC base `152654e5a0e2d84d4c2f245ea7590488f0497430` con cambios locales; se mantienen las restricciones generales y la tarea completa.
- `src/Frontend/Bodies/StatementAnalyzer.cpp:4610-4622` rechaza explícitamente un local C++ por valor, mientras que permite analizar `ref`; `:4277` consulta `typeRequiresValueDrop` y `:4445-4456` registra la región de cleanup cuando el valor lo requiere. `MemberModel/OwnershipQueries.cpp:105-149` deriva ese requisito de los miembros de tipo struct. Debe verificarse con un modelo real importado que el destructor externo esté presente antes de registrar el local y que el control de copia/movimiento represente sus reglas C++.
- `src/Frontend/ProjectObjectLayoutModel.cpp:115-121` excluye los records importados de `structLayouts`, pero `:288-295` expone tamaño y alineación de Clang mediante `valueLayoutForTypeKey`; `MIR/MirLayoutImporter.cpp:importNamedStructLayouts` importa los layouts de esa colección. Por tanto el tamaño observado no basta para garantizar una ranura local o glue de destrucción: es necesario conectar una representación física del record sin reinterpretar sus campos como un struct NativeSharp ni permitir copias triviales indebidas.
- `src/LLVM/Lowering/OopLlvmConstruction.cpp:lowerValueConstruction` crea actualmente su propia ranura, invoca el constructor y, salvo la ruta descartada, carga el carrier. `OopLlvmStatements.cpp:lowerLocalDeclarationStatement` baja el inicializador y guarda el valor en la ranura local. Para un C++ no trivial, esa ruta puede copiar bytes a otra dirección; se necesita construcción directamente en el almacenamiento final, y activar el drop solo tras éxito.
- `src/LLVM/Lowering/OopLlvmCleanup.cpp:emitValueDropAddress` busca un layout MIR y retorna sin destruir si no encuentra uno; eliminar únicamente la barrera semántica podría omitir silenciosamente el destructor. La limpieza ordinaria y la excepcional usan regiones activas; la prueba debe cubrir alcance, return, excepción posterior, excepción en constructor, orden inverso y ausencia de copias, además de O0/O2 y almacenamiento alineado. Estos son requisitos de implementación, no capacidades verificadas. Siguiente acción: diseñar el layout importado y el destino in situ como un cambio coherente junto al plan de destructor, con pruebas de ejecución antes de activar la ruta ordinaria. Sin cambios de código ni commit.
- Alcance de construcción incremental: `ProjectCompilation.h/.cpp`, `ProjectMethodBodyModel.h`, `Bodies/MethodBodyCallableLifecycle.cpp`, `Bodies/StatementAnalyzer.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Añadir una opción aislada de prueba que permita examinar semánticamente un local C++ por valor y su región de cleanup, pero rechace avanzar a layout y sea incompatible con el modo de emisión de temporales descartados. La ruta de producción y la barrera de locales por valor permanecen iguales. Comprobar con una construcción real si el local requiere destrucción y si la región está asociada al ordinal correcto; no inferir ejecución ni habilitar la opción desde el driver. Construir ON/OFF y ejecutar la suite y regresiones pertinentes, respetando todos los límites generales.
- Resultado 2026-11-05: `enableExperimentalCppLocalLifetimeAnalysisForTests` propaga una opción explícita a los fragmentos del análisis y conserva una barrera antes de layout; no se activa desde el driver y no se puede combinar con el modo de temporales descartados. La suite analiza `Demo.Ready value = new Demo.Ready(1)`, comprueba que el local requiere drop y que una región `ValueLocalDrop` apunta a su ordinal, y confirma la barrera y la incompatibilidad en el orden probado. El primer control de barrera falló porque reutilizaba `ProjectCompilation`, que es de un solo uso; con una instancia nueva pasó. Construcciones ON `376b48a6753efaec` y OFF `c7d78697b138be80`, `pruebas-lifetime/ejecutar.cmd`, referencias locales, construcción descartada y `git diff --check`: correctos. El test no genera ni ejecuta un local C++ por valor; el layout, construcción in situ, llamada al destructor, excepciones y el orden opuesto de opciones siguen sin verificar. Cambios sin commit.
- Siguiente conexión experimental: `ProjectCompilation.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Permitir que la opción aislada de local alcance el modelo de layout, pero detenerla antes de construir HIR; mantener la opción incompatible con generación de temporales y la barrera del driver. Comparar el tamaño y la alineación consultados para el local real con la declaración importada desde Clang, y confirmar que no se fabrica un `StructValueLayout` NativeSharp. Comprobar el bloqueo HIR y el rechazo de la ruta ordinaria; construir ON/OFF y correr suite y regresiones. No afirmar almacenamiento, constructor ni destructor ejecutables.
- Resultado 2026-11-05: el proyecto experimental con `Demo.Ready value = new Demo.Ready(1)` llega al layout; el tamaño y la alineación consultados para ese local coinciden con la declaración de Clang y `structLayoutForType` sigue ausente. La región semántica de destrucción permanece asociada al local. La opción se detiene antes de HIR y no puede combinarse con el modo de temporales descartados. La ruta normal sigue rechazando el local por valor. NSC ON `1802175a99420524` y OFF `5a89f9324a3cf3d0` construyeron; la suite de lifetime, referencia local, constructor descartado y `git diff --check` pasaron. Aún no se reserva la ranura real, no se emite el constructor ni se ejecuta el destructor del local. Cambios sin commit.
- Siguiente conexión experimental: `ProjectCompilation.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Permitir que el local aislado llegue a HIR y desplazar la barrera inmediatamente antes de MIR; comprobar que la declaración conserva el ordinal, la región de limpieza y la construcción seleccionada. Seguir bloqueando el modo de producción y la combinación con temporales descartados. No emitir LLVM ni afirmar que el local puede ejecutarse; preservar regresiones y construir ON/OFF.
- Resultado: construcciones ON y OFF correctas; el contrato de runtime estático pasó en ambas. La suite invocada mediante `pruebas-lifetime/ejecutar.cmd` pasó y verificó que el local alcanza HIR con identidad de construcción y región de limpieza, y que la barrera impide continuar a MIR. Las regresiones de referencia local y lifetime explícito pasaron en O0/O2; `git diff --check` pasó. Una ejecución directa sin argumentos de la suite devolvió 2 porque requiere parámetros; el script sí la ejecutó con sus argumentos. No se ha demostrado generación de almacenamiento ni destrucción de locales C++ por valor. Sin commit.
- Investigación siguiente: `MirNodeMachine.cpp` transfiere a MIR el ordinal del local y el de la región de limpieza, y tiene una ruta específica para la construcción de tipos C++ importados. Esto no demuestra que el backend construya directamente en el almacenamiento del local ni que la limpieza llame su destructor. Esos contratos requieren pruebas separadas antes de retirar la barrera; evitar copias de bytes de objetos no triviales y destrucción de objetos cuyo constructor falló.
- Prueba aislada siguiente: construir un `ProjectOopMirModel` separado a partir del HIR de `Demo.Ready value = new Demo.Ready(1)`, verificando que local, región y constructor alcanzan MIR. Modificar solo `tests/unit/CppClangSessionTests.cpp` y esta bitácora; no alterar el compilador, la barrera de MIR ni el driver. Ejecutar la suite mediante el script de lifetime. Esta observación no demuestra generación LLVM ni corrección del ciclo de vida.
- Resultado: la construcción de la suite y `pruebas-lifetime/ejecutar.cmd` terminaron correctamente. El MIR aislado conserva el local, la construcción con símbolo y dimensiones, y una región de destrucción automática cuyos ordinales y tipo coinciden con el local. La prueba no pasa por el driver ni habilita emisión: todavía falta relacionar esa limpieza con el destructor C++ y construir sin copiar. Sin commit.
- Siguiente reproducción: bajar el MIR aislado a LLVM con el modelo de miembros y buscar construcción y destrucción del local, sin quitar la protección del driver. La hipótesis nace de `collectLocalStorage` (tipo/alineación), `lowerLocalDeclarationStatement` (almacena un valor producido aparte), `prepareFunctionCleanupState` (bandera de vida solo para tipos clasificados con destructor propio) y `emitValueDropAddress` (omite tipos sin layout struct MIR). Presupuesto inicial: `tests/unit/CppClangSessionTests.cpp` y bitácora; la prueba debe mostrar la falla real antes de modificar generación. Compilador normal y pruebas existentes protegidos.
- Resultado: la suite experimental llegó a la emisión LLVM aislada y falló con `Stage 40 cannot map named value type to LLVM carrier: type:Semantic::Demo.Ready`. La falla ocurre al preparar almacenamiento, antes de que pueda verificarse el constructor o el destructor. Se retiró la aserción exploratoria que fallaba para no dejar la suite permanentemente en rojo; el test MIR y la barrera del driver permanecen. Siguiente paso: diseñar almacenamiento de bytes con el tamaño y alineación de Clang exclusivamente para el local C++ identificado, junto con construcción en destino y limpieza, sin extrapolarlo a capturas, estáticos o corutinas sin pruebas. Sin commit.
- Tras retirar la aserción exploratoria se reconstruyó la suite y `pruebas-lifetime/ejecutar.cmd` volvió a terminar con código 0; `git diff --check` pasó. El fallo LLVM anterior sigue abierto y no se considera una corrección.
- Implementación experimental siguiente: `OopLlvmFunctionDefinition.cpp`, `OopLlvmConstruction.cpp`, `OopLlvmStatements.cpp`, `OopLlvmCleanup.cpp`, `OopLlvmImplementation.h`, `CppClangSessionTests.cpp` y esta bitácora. Para el local C++ automático ya representado en MIR, reservar bytes con dimensiones importadas, pasar esa misma dirección al constructor y llamar al destructor externo durante cleanup; rechazar explícitamente capturas, locales estáticos o inicializadores distintos que no estén cubiertos. Mantener cerradas las barreras del driver y de `compileProjectOopMir`. La prueba aislada debe verificar el IR y la suite previa; no declarar el local usable hasta probar ejecución, excepciones y las demás formas de vida.
- Resultado intermedio: compilación ON y `pruebas-lifetime/ejecutar.cmd` correctas; el LLVM aislado verifica y contiene llamadas al constructor y destructor sin `struct.new.value`. Esto no demuestra que las direcciones coincidan ni valida excepciones. Siguiente verificación: emitir objetos O0/O2 y enlazarlos con un arnés C++ que registre y compare las direcciones del constructor y destructor después de regresar de NSC. Presupuesto adicional: `pruebas-lifetime/ejecutar.cmd`, el fixture generado por la suite y salidas en `A:/nsc-interop-build/cpp-fixtures/lifetime-emission`; conservar las pruebas existentes.
- Resultado de ejecución: `pruebas-lifetime/ejecutar.cmd` pasó en O0/O2 con el local `Demo.Ready value = new Demo.Ready(1)`: el arnés C++ comprobó el argumento del constructor, la misma dirección en destructor y una transición de estado compatible con una sola destrucción antes de regresar. Construcciones ON/OFF y contrato de runtime estático correctos; regresiones de referencia local y valor descartado pasaron y `git diff --check` pasó. La generación parte de MIR aislado, no de un proyecto admitido por el driver. Persisten las barreras de locales; no se han probado excepciones del constructor o destructor, múltiples locales, bloques anidados, capturas, static, corutinas ni copias y movimientos. Siguiente paso: verificar unwinding real tras construir y cuando falla el constructor, con contador de destructor y orden observable, antes de plantear la habilitación ordinaria. Sin commit.
- Experimento siguiente: dos locales `Demo.Risky`, cuyo segundo constructor lanza una excepción C++; un arnés captura y verifica destrucción única del primer objeto y ausencia de destrucción del segundo incompleto. Usar HIR/MIR aislados, emitir O0/O2, enlazar con Clang/LLD y el runtime estático autorizado. Presupuesto: `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, archivos generados en la carpeta de lifetime y esta bitácora. No modificar el backend ni retirar protecciones para hacer pasar la prueba; conservar la evidencia de una falla.
- Resultado: la suite construyó un proyecto experimental con dos locales `Risky`; el segundo constructor lanzó `42` y el arnés C++ comprobó en O0/O2 la captura del valor, una sola llamada al destructor del primer objeto y ninguna al segundo. `pruebas-lifetime/ejecutar.cmd`, referencias locales, lifetime explícito y `git diff --check` terminaron con código 0. No se modificó el backend ni se habilitó el driver. Quedan pendientes una excepción después de ambas construcciones, orden inverso de varios objetos completos, destructor que lanza y las demás formas de vida; no generalizar a todas las excepciones. Sin commit.
- Siguiente experimento: ampliar el mismo proyecto aislado a tres locales; ejecutar sucesivamente el fallo en el segundo y en el tercero. Comprobar que nunca se destruya el objeto incompleto y que los completos se destruyan una sola vez en orden inverso. Alcance: `tests/unit/CppClangSessionTests.cpp`, sus fixtures generados y esta bitácora. No cambiar el backend ni retirar barreras para hacer pasar el experimento; ejecutar la suite de lifetime en O0/O2 y conservar fallos.
- Resultado: la suite pasó en O0/O2 para ambos puntos de fallo, comprobando la excepción `42`, el número de destructores y que sus direcciones correspondan a los objetos completos en orden inverso. Esta evidencia usa el constructor del tercer local como origen de la excepción y no cubre todavía una llamada posterior ni el retorno normal con tres objetos vivos. Sin cambios del backend ni commit.
- Siguiente comprobación: añadir al mismo arnés una ejecución sin excepción y comprobar la destrucción inversa de los tres locales al retornar, manteniendo los dos escenarios anteriores. Presupuesto: `CppClangSessionTests.cpp`, fixtures generados y bitácora; reconstruir la suite y ejecutar el mismo script.
- Resultado: la suite reconstruida y `pruebas-lifetime/ejecutar.cmd` terminaron con código 0. En O0/O2 se verificaron los dos puntos de fallo y el retorno normal con tres objetos, comprobando las direcciones y el orden inverso de sus destructores. `git diff --check` pasó para NSC y el fork. Todavía no se ha probado una excepción procedente de otra función después de completar los objetos, ni excepciones de destructores; el driver permanece protegido. Sin commit.
- Siguiente prueba: añadir una llamada importada a función C++ después de construir los tres locales, con un escenario que lanza y otro que retorna. Mantener los fallos de constructor y comprobar la limpieza inversa en O0/O2. Presupuesto: `CppClangSessionTests.cpp`, fixtures generados y bitácora; sin tocar el backend ni quitar protecciones. El arnés que captura la excepción solo verifica el desenrollado.
- Resultado: la suite reconstruida y `pruebas-lifetime/ejecutar.cmd` pasaron en O0/O2 para cuatro recorridos: fallo del segundo constructor, fallo del tercero, excepción `42` lanzada por `cpp_local_throw` tras construir los tres y retorno normal de esa función. El arnés comprobó número y direcciones de destructores en orden inverso. Ambos `git diff --check` pasaron. La llamada es una función implementada en C++ con enlace C, proyectada desde Clang; la captura C++ solo instrumenta la prueba. El proyecto sigue generado mediante MIR aislado y no demuestra manejo de destructores lanzadores, DLL ni habilitación del driver. Sin commit.
- Siguiente experimento: enlazar los objetos NativeSharp aislados ya producidos por la suite con un constructor, destructor y función que lanza implementados en una DLL C++, y repetir los cuatro recorridos en O0/O2. Presupuesto: fixtures nuevos y script bajo `pruebas-lifetime`, salidas en `A:/nsc-interop-build/cpp-fixtures/lifetime-emission` y bitácora; no modificar el backend. Ejecutar primero la suite para regenerar los objetos y comprobar el símbolo de entrada. El resultado solo cubre esta configuración DLL y mantiene protegida la ruta de producción.
- Resultado: `pruebas-lifetime/ejecutar-local-dll.cmd` regeneró la suite y construyó una DLL con CRT estático. En O0/O2 los cuatro escenarios pasaron: fallo en segundo o tercer constructor, excepción en llamada posterior y retorno normal, con orden inverso y direcciones verificadas dentro de la DLL. `llvm-readobj` confirmó que ambos ejecutables importan de `local-throw.dll` el constructor, destructor y `cpp_local_throw`; los listados se conservan en `local-throw-dll-imports-O0.txt` y `local-throw-dll-imports-O2.txt` en la carpeta de salida. Los objetos NS fueron emitidos desde el MIR aislado con máquina de destino O0/O2; esto no demuestra una compilación ordinaria ni una matriz de DLL y runtimes completa. No se alteró el backend. Sin commit.
- Siguiente implementación: ampliar el planificador de emisión de miembros implícitos, que solo reconoce `ExpressionStatement`, para reconocer también construcción directa de un local C++ con identidad de cleanup y tipo coincidentes. Deduplicar las mismas solicitudes y rechazar bindings inconsistentes. Renombrar la función para reflejar ambos usos y actualizar sus llamadas. Alcance: `CppSemanticFunction.h/.cpp`, `DriverArtifactGeneration.cpp`, `CppClangSessionTests.cpp` y bitácora; barreras del driver y semántica intactas. Verificar con MIR aislado real que un local implícito solicita constructor y destructor y que la prueba anterior de valores descartados sigue pasando; reconstruir ON/OFF y ejecutar lifetime.
- Resultado: `planCppImplicitLifetimeFromValues` reconoce construcción directa en local además de valores descartados, valida identidad de local/cleanup/tipo y reutiliza la deduplicación y validación de los miembros. La suite construyó MIR aislado con dos locales `Demo.Implicit` y verificó exactamente una solicitud de constructor y una de destructor. NSC ON/OFF construyó con contrato estático; `ejecutar-local-dll.cmd` y la regresión de lifetime explícito pasaron, igual que `git diff --check`. Las solicitudes de locales no se han ejecutado todavía mediante el bundle ni el driver; la barrera de locales permanece. Siguiente paso: emitir y enlazar estos cuerpos implícitos junto con los objetos del local y probarlos antes de conectar el driver. Sin commit.
- Siguiente prueba: usar dos locales `Demo.Observed` con constructor/destructor implícitos no triviales; emitir desde su plan los cuerpos de Clang y el objeto NS aislado en O0/O2. Un arnés C++ registrará construcción y destrucción de sus subobjetos, comprobará direcciones y orden inverso. Presupuesto: `CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, salidas generadas y bitácora; no cambiar backend ni protecciones, y mantener las pruebas de temporales y DLL.
- Resultado: el plan de dos locales `Demo.Observed` produjo una solicitud por miembro implícito; la suite emitió los cuerpos Clang y el objeto NS aislado, los enlazó con un arnés C++ y ejecutó O0/O2. El arnés verificó dos construcciones y destrucciones de cada subobjeto, igualdad de direcciones y destrucción inversa de objetos y miembros. La primera versión del arnés omitía `noexcept(false)` en sus declaraciones; se corrigió para coincidir con el header, se reconstruyó y se repitió `ejecutar-local-dll.cmd` con código 0, incluyendo las regresiones existentes. Ambos `git diff --check` pasaron. El enlace es aislado: el bundle y el driver ordinario aún no se han probado con estos locales. Sin commit.
- Siguiente comprobación: generar el bundle NSC a partir del MIR aislado de los dos locales, agregar mediante `appendCppImplicitLifetimeObjects` los cuerpos implícitos y archivarlo con el productor habitual. Enlazar y ejecutar el arnés existente contra ese archivo O0/O2; verificar que se agreguen exactamente dos objetos y que sus temporales se limpien. Presupuesto: `CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, salidas de prueba y bitácora. Mantener las barreras de producción y no modificar el driver para hacer pasar la prueba.
- Resultado: en O0/O2 el generador creó el bundle para dos locales `Observed`; `appendCppImplicitLifetimeObjects` agregó exactamente dos objetos y el destructor del bundle eliminó sus temporales después de archivarlos. El arnés enlazado contra cada `.lib` comprobó direcciones y destrucción de los miembros en orden inverso. `llvm-nm --defined-only` confirmó `??0Observed@Demo@@QEAA@XZ` y `??1Observed@Demo@@QEAA@XZ` en ambos archivos. `ejecutar-local-dll.cmd` y `git diff --check` pasaron. El MIR se construyó de forma aislada y el driver ordinario aún bloquea locales; no se han validado las demás formas de construcción, copia y movimientos. Sin commit.
- Siguiente conexión experimental: permitir que la opción explícita de prueba para locales y la de descartados coexistan y lleguen por `ProjectCompilation` a MIR. En HIR comprobar que todo local C++ por valor tenga construcción directa, almacenamiento automático y cleanup; rechazar accesos que produzcan o transfieran un valor C++ mientras no se modelen sus copias. La ruta ordinaria del driver sigue sin activar la opción de locales. Presupuesto: `ProjectCompilation.cpp`, `CppClangSessionTests.cpp` y bitácora; verificar ambas configuraciones de compilación, caso positivo por la ruta real, rechazos y regresiones lifetime. No presentar un rechazo como interoperabilidad completada.
- Resultado: con ambas opciones experimentales activadas, un proyecto que contiene un local `Ready` y un valor descartado alcanza MIR mediante `ProjectCompilation`; un intento de inicializar un segundo local copiando el primero no compiló, aunque la prueba no distingue todavía la fase o diagnóstico exacto de rechazo. La comprobación HIR exige construcción directa y cleanup para el local y no admite cargas/transferencias de records por valor en esta ruta. Construcciones ON/OFF y contrato estático correctos; pasaron la suite de lifetime/DLL, referencias locales, temporales explícitos y `git diff --check`. El driver no activa la opción de locales; el caso combinado todavía no se enlazó ni ejecutó por esta ruta. Sin commit.
- Siguiente prueba: emitir el MIR del proyecto combinado mediante el generador de objetos y enlazar un arnés C++ que observe las dos construcciones y su orden de destrucción. Registrar el diagnóstico real del intento de copia antes de concretar su aserción. Presupuesto: `CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, salidas de prueba y bitácora; sin cambios en el backend ni activación del driver. Ejecutar O0/O2 y conservar regresiones.
- Resultado: un proyecto con un local `Ready` y un temporal descartado llegó a MIR mediante `ProjectCompilation` con ambas opciones experimentales, generó el bundle y una biblioteca NSC; el arnés enlazado comprobó O0/O2 dos direcciones distintas y destrucción primero del temporal y después del local. El primer enlace falló por `__C_specific_handler` ausente; al incluir la biblioteca de importación SEH de la prueba, el enlace y la suite completa `ejecutar-local-dll.cmd` pasaron. La copia se rechaza en análisis semántico con `move-only value ... cannot be copied into local 'b'`; la prueba exige ese diagnóstico. `git diff --check` pasó en ambos repositorios. La habilitación automática del driver, copia/movimiento C++ y otros casos siguen pendientes. Sin commit.
- Siguiente experimento: permitir que el destructor de `Risky` sea `noexcept(false)` y añadir un escenario en el que el segundo destructor de tres objetos construidos lance `43` durante retorno normal. Comprobar que el primero aún se destruye exactamente una vez y la excepción llega al arnés. Presupuesto: `CppClangSessionTests.cpp`, `pruebas-lifetime/LocalThrowDll.cpp` para mantener la firma coherente, fixtures generados y bitácora; sin cambios de backend ni barreras. Correr O0/O2 y registrar cualquier fallo antes de corregirlo.
- Resultado intermedio: el arnés enlazado estáticamente pasó en O0/O2; al lanzar el destructor del segundo objeto se llamó al destructor del primero y se capturó `43`. Los cuatro escenarios previos en DLL también pasaron con la firma de destructor actualizada. Siguiente comprobación: extender `LocalThrowDll.cpp` y `LocalThrowDllHarness.cpp` para lanzar y comprobar `43` desde la DLL, sin alterar el backend; repetir el script completo.
- Resultado: `ejecutar-local-dll.cmd` pasó en O0/O2 con cinco escenarios, incluido el destructor intermedio que lanza desde la DLL durante retorno normal. El arnés verificó la excepción `43`, tres destrucciones en orden inverso y direcciones correctas. También pasaron las pruebas estáticas y ambos `git diff --check`. No se probó que un destructor lance durante el desenrollado de otra excepción, ni se habilitó el driver; no extrapolar el resultado a esos casos. Sin commit.
- Siguiente experimento: comparar Clang C++ puro con el objeto NS aislado cuando un destructor lanza durante el desenrollado de una excepción, usando un controlador de terminación que termina con código observable en un proceso separado. Construir un control C++ puro y un consumidor del objeto NS con la DLL de prueba; registrar los códigos en O0/O2 y no interpretar una falla como cierre. Presupuesto: `pruebas-lifetime/LocalThrowDll.cpp`, dos fixtures y script nuevos bajo `pruebas-lifetime`, salidas de prueba y bitácora; backend y driver protegidos.
- Resultado parcial reproducido en O0: el control Clang terminó mediante `set_terminate` con código 77; el caso NS propagó la segunda excepción `43` al `catch` y terminó con código 43. Esto contradice el control C++ y bloquea habilitar locales ordinarios. La primera prueba no enlazó porque se declaró erróneamente `std::set_terminate` como función propia; el header UCRT expone el símbolo global con enlace C y se corrigió el arnés. No se ha llegado a O2 ni se ha modificado el backend. Siguiente acción: identificar la emisión de cleanup excepcional y contrastarla con el IR de Clang, conservando la prueba roja; no asumir que el problema provenga solo del runtime estático de la DLL. Sin commit.
- Resultado O2: control Clang código 77, caso NS código 43, igual que O0. En el IR del control, Clang ejecuta el destructor dentro de un `cleanuppad` bajo `__CxxFrameHandler3`; `buildPotentiallyThrowingCall` de NSC utiliza el destino de unwind de su cadena de limpieza. Esa diferencia es evidencia de investigación, no causa demostrada. Siguiente control: compilar C++ puro usando la misma DLL `Risky` y el mismo fallo doble para separar el marco NS de las bibliotecas de runtime. Presupuesto adicional: `LocalDoubleException.cpp`, script de prueba y bitácora; backend protegido mientras se identifica la causa.
- El control C++ puro con la misma DLL terminó con código 77 en O0; el caso NS con esa DLL devolvió 43. Hipótesis estrecha: la llamada del destructor C++ desde un `cleanuppad` sigue un destino de unwind distinto del `call` emitido por Clang. Probar exclusivamente en `OopLlvmCleanup.cpp:emitValueDropAddress` una llamada con bundle de funclet en vez de `invoke` cuando el destructor externo C++ se ejecuta en cleanup excepcional; no alterar cleanup NativeSharp ni la ruta de salida normal. Conservar el cambio solo si el control y NS coinciden en O0/O2 y pasan las regresiones de destructor durante retorno, constructor y DLL; en caso contrario revertirlo. Presupuesto: ese símbolo, tests/scripts existentes y bitácora.
- Resultado: el control C++ con la misma DLL devolvió 77. Cambiar únicamente el destructor C++ de `invoke` a `call` dentro del cleanup NS no corrigió la divergencia: `double-ns-O0.exe` devolvió 43. Se retiró ese cambio de backend; la prueba diferencial permanece fallando y el driver sigue protegido. Es necesario investigar una frontera de terminación explícita o el modelo de personalidad de excepciones, sin alterar semántica de cleanup NativeSharp ni presumir que el nombre `buildNoUnwindCall` impida propagación. No se ha aplicado corrección. Sin commit.
- Corrección de proceso: se creó `pruebas-lifetime/NoexceptBoundaryReference.cpp` para observar el IR de una función C++ `noexcept` antes de registrar este alcance, por lo que el registro previo quedó omitido. Se conserva el archivo como evidencia de la desviación. Alcance de la observación: compilar solo ese fixture a IR en la carpeta de pruebas y comparar su mecanismo de terminación; sin modificar compilador ni atribuirle todavía corrección al caso NS.
- Evidencia adicional: Clang 23 produjo `noexcept-boundary.ll` con función `nounwind`, personalidad `__CxxFrameHandler3`, `invoke` al destructor indirecto y `cleanuppad` que llama `__std_terminate`. Esto permite explorar una frontera C++ únicamente para el destructor ejecutado durante unwind; aún no se ha integrado, enlazado ni ejecutado esa alternativa y debe evaluarse su dependencia estática y comportamiento con DLL. Tras revertir la hipótesis previa se reconstruyó NSC y `ejecutar-local-dll.cmd` volvió a pasar; la prueba diferencial de doble excepción continúa roja. Sin commit.
- Experimento siguiente: compilar una variante de `LocalThrowDll.cpp` que llame la frontera `noexcept` solo en el escenario de doble excepción y enlazarla con el fixture de Clang; ejecutar contra el control C++ y el objeto NS aislado en O0/O2, comprobando código 77. Presupuesto: ese fixture, script nuevo bajo `pruebas-lifetime`, salidas en la carpeta de pruebas y bitácora; mantener la DLL original y el backend intactos. El experimento valida la frontera en la DLL, no su integración en NSC.
- Resultado parcial: control C++ con DLL terminó con 77, pero la variante con NS terminó con `0xC0000409` al ejecutar la frontera dentro de la DLL. Hipótesis por comprobar: cada módulo usa CRT estático y el controlador instalado en el ejecutable no es el controlador del runtime que ejecuta `__std_terminate` en la DLL. Siguiente prueba: registrar el mismo marcador también en la DLL experimental y repetir control/NS, sin alterar la DLL normal ni el backend. Archivos adicionales: `LocalDoubleException.cpp` y el script experimental.
- Resultado: `ejecutar-frontera-noexcept.cmd` terminó con código 0: control C++ y objeto NS terminaron mediante el marcador 77 en O0/O2 al instalarlo en ejecutable y DLL y ejecutar la frontera en la variante experimental de la DLL. Sin frontera, `double-ns-O0.exe` siguió devolviendo 43. Esto muestra que la frontera puede forzar el contrato en este montaje y que importa qué runtime recibe el controlador; no prueba una solución integrada en NSC. `llvm-nm --undefined-only boundary.obj` identificó `__CxxFrameHandler3` y `__std_terminate`, dependencias que deben resolverse al diseñar la integración sin imponer CRT ocultamente. La suite normal y `git diff --check` pasaron. Siguiente acción: estudiar cómo generar/enlazar la frontera y conservar la semántica y el runtime apropiados para destructor C++ durante unwind, con control de fallos y sin modificar los destructores nativos. Sin commit.
- Siguiente implementación experimental: generar en el módulo LLVM una frontera interna equivalente al IR `noexcept` de Clang y usarla solo para destructores C++ potencialmente lanzadores durante cleanup excepcional. La frontera declara explícitamente `__CxxFrameHandler3` y `__std_terminate`, invoca el destructor y termina si lanza; la salida normal conserva su ruta. Mantener el driver de locales protegido, no modificar destructores NativeSharp y verificar la dependencia explícita en los objetos y el enlace estático de las pruebas. Presupuesto: `OopLlvmCleanup.cpp`, `OopLlvmImplementation.h` para conservar la identidad de la función por módulo, `CppClangSessionTests.cpp` si hace falta comprobar IR, scripts de pruebas afectados y bitácora. Comparar el caso diferencial con Clang en O0/O2, constructor fallido, destructor durante retorno normal, DLL y regresiones; no habilitar producción si alguna falla.
- Resultado: NSC genera una frontera LLVM interna con personalidad C++ y `__std_terminate` únicamente cuando un destructor C++ potencialmente lanzador se ejecuta en cleanup excepcional; valida existencia y tipos de los símbolos. `ejecutar-doble-excepcion.cmd` pasó en O0/O2 con control Clang, control Clang con DLL y el marco NS: todos invocaron el marcador 77. La suite incluida también pasó el destructor lanzador durante retorno normal, constructor fallido, orden de destrucción y DLL. Construcciones ON/OFF y contratos estáticos, regresiones de referencias, temporales, `finally` y `raii` NS, y `git diff --check`: correctos. `llvm-nm` confirma dependencias no resueltas `__CxxFrameHandler3` y `__std_terminate` en el objeto que requiere esa frontera, resueltas explícitamente por el CRT estático en estas pruebas; la política y distribución/enlace del runtime para proyectos ordinarios aún deben conectarse y verificarse. El driver de locales permanece protegido. Sin commit.
- Siguiente verificación: enlazar los mismos objetos de doble excepción con `nslink.exe`, pasando explícitamente el CRT estático autorizado y bibliotecas de importación de Windows; comparar con el resultado LLD en O0/O2, registrar dependencias PE y cualquier fallo. Presupuesto: script nuevo bajo `pruebas-lifetime`, salidas de prueba y bitácora; no modificar NSLink ni configurar el driver de locales. Ejecutar primero la prueba diferencial para regenerar los objetos.
- Resultado: LLD enlazó y ejecutó la comparación en O0/O2, pero `C:/native-sharp/bin/nslink.exe` falló al enlazar O0 con `NSL1023` por `_guard_dispatch_icall` en `dispatchcfg.obj` de `libcmt.lib` y `libucrt.lib`; al invertir el orden se reprodujo el mismo conflicto con ganadores intercambiados. Este hallazgo coincide con la reproducción anterior de la sección de bundle y con el caso mínimo de extracción de archivos ya documentado; no demuestra por sí solo que esa sea la causa exacta aquí. No se generó ni ejecutó el PE NSLink ni se probó O2 con NSLink. El primer script mostraba código de salida 0 pese al fallo dentro de `for`; se cambió a un salto a etiqueta fuera del bloque y la repetición mostró el diagnóstico y devolvió 1. El log está en `A:/nsc-interop-build/cpp-fixtures/lifetime-emission/double-nslink-build-O0.log`. NSLink permanece protegido y sin cambios; el enlazado ordinario con CRT para este caso sigue pendiente. Sin commit.
- Siguiente implementación: habilitar desde el driver únicamente el local C++ automático con construcción directa y constructor/destructor inequívocos `noexcept`, preservando las barreras para copia, movimiento, estáticos y miembros potencialmente lanzadores; la opción experimental conserva sus escenarios más amplios. Este subconjunto no requiere la nueva frontera CRT y se verificará con NSLink mediante proyecto real, alcance anidado, O0/O2 y caché deshabilitada. Archivos: `ProjectCompilation.h/.cpp`, `PrecompiledModuleConfiguration.cpp`, fixture nuevo bajo `pruebas-proyecto/lifetime-local`, `pruebas-proyecto/lifetime-explicito/Unsupported.ns.txt` y bitácora. La antigua prueba `Unsupported` rechazaba el local que ahora se habilita: se cambia su entrada a una copia aún no soportada, sin suprimir la aserción de rechazo. Conservar regresiones de temporales, referencias y excepciones; no modificar NSLink ni afirmar cobertura general.

## Investigación incremental: referencias locales a objetos C++ (2026-11-05)

- Objetivo: determinar si un objeto C++ proporcionado por una biblioteca puede utilizarse desde una referencia local NativeSharp sin asumir propiedad ni copiarlo, como paso hacia variables locales C++ completas. Alcance inicial: inspección de NSC y fixture aislado bajo `A:/nsc-interop-build/cpp-fixtures/` más esta bitácora; no modificar el compilador hasta conocer la semántica y las rutas. Se mantiene el rechazo de locales por valor y las restricciones generales.
- Resultado inicial: con `ref Demo.Widget value = ref *p` y `p = Demo.make()`, un proyecto real compiló y devolvió 26 al escribir `value.value` y llamar `value.add(2)`. `Demo.make()` devuelve la dirección de un `static Widget` de la biblioteca, según `pruebas-proyecto/CppClassApi.cpp`; este caso usa un tipo trivial y no demuestra destrucción, comprobación de duración ni otros modos de propiedad.
- Extensión de evidencia: agregar un fixture permanente en `pruebas-proyecto/referencia-local-cpp` para comprobar que la escritura mediante la referencia se observa mediante el puntero original, la llamada de método, ejecución O0/O2 y rechazo existente de local por valor. Archivos adicionales: tracking y bitácora. Sin cambios al compilador.
- Resultado: la prueba permanente ejecutó O0 y O2 con resultado 26 y confirmó que el puntero original observa la escritura de la referencia; el control de local C++ por valor fue rechazado. Como el objeto original es trivial, la prueba no demuestra si el destructor se evita. Siguiente prueba dentro de la misma carpeta: proporcionar un objeto C++ de destructor no trivial con propiedad retenida por la biblioteca y contador observable, y comprobar que salir del ámbito de la referencia no lo destruye. Se preserva el control por valor y no se altera NSC.
- Primer intento con destructor observable: el identificador C++ `destroy` produjo errores de parser por ser una palabra reservada; la herramienta ejecutora reportó código 0 pese a que la salida terminó en esos errores y no apareció la confirmación final del script. Se cambió el nombre del fixture a `releaseObject` y la prueba pasó, pero se debe reproducir y aclarar la discrepancia de código de salida y evitar ejecutables anteriores en la batería antes de dar por cerrada la evidencia. Alcance adicional de diagnóstico: fixtures y script de `referencia-local-cpp`, artefactos en `cpp-fixtures/cpp-local-reference` y esta bitácora; no modificar NSC hasta identificar la ruta causal.
- Evidencia final: con `BorrowedDemo.Widget` de destructor no trivial, la biblioteca construye el objeto en su almacenamiento y entrega un puntero. En O0 y O2, una referencia `ref` escribe en él, llama un método y sale de ámbito; el contador de destrucción sigue en cero. La función de la biblioteca `releaseObject` llama explícitamente al destructor y el contador llega a uno; el programa devuelve 26. El script borra cada ejecutable de prueba antes de compilarlo y exige su recreación, y mantiene el rechazo de un local C++ por valor. El error sintáctico aislado reportó `NSC_EXIT=1` y no produjo ejecutable; no se identificó por qué la herramienta había reportado código 0 en el intento anterior. La evidencia no cubre punteros colgantes, concurrencia, DLL, todas las salidas ni la propiedad de valores C++ construidos por NativeSharp. Ningún cambio de compilador ni commit.

## Investigación incremental: causa del enlace mixto de importación COFF larga (2026-11-05)

- Objetivo: comparar el mismo objeto largo con una importación corta de Win32 bajo LLD y NSLink y determinar por qué el PE NativeSharp omitió `foo.dll`. Alcance inicial: fixtures en `pruebas-proyecto/importacion-coff-larga`, artefactos en `A:/nsc-interop-build/cpp-fixtures/importacion-coff-larga` y esta bitácora; sin modificar compilador, LLVM ni archivos en C:. La barrera para el formato largo permanece. Se compararán tablas PE y ejecución antes de proponer corrección.
- Resultado: `comparar-enlace.cmd` enlaza el mismo `mixed-main.obj`, `long.lib` y `kernel32-test.lib` con LLD 23.1.0 y NSLink 3.0.0. `llvm-readobj --coff-imports` muestra `foo.dll` y `KERNEL32.dll` en el PE de LLD, que devuelve 42; el PE de NSLink solo declara `KERNEL32.dll`. No se ejecutó este PE defectuoso. El script devuelve código 0 cuando reproduce esa diferencia; no significa que la capacidad esté implementada.
- Inspección sin cambios de `A:/nslink-devel` en revisión `da19a58028404ea5eef467418deefce6f8fd8592`: `src/link/ArchiveResolver.cpp:111-151` transforma miembros de importación corta en `ImportDescription`, mientras que en líneas 154-170 los demás miembros entran como objetos COFF ordinarios; `src/link/LinkPipeline.cpp:156-170` alimenta el constructor sintético con la lista de importaciones y `src/pe/PeImportBuilder.cpp:393` fija el directorio al bloque sintético. Este recorrido es compatible con el fallo observado, pero no se ha probado que el binario `nslink` del PATH provenga exactamente de esa revisión ni se ha demostrado toda la secuencia de layout. Una solución requiere resolver la incorporación de descriptores largos al PE o usar una ruta de enlace que los soporte; no se modificó el repositorio NSLink, que no figura entre las zonas de escritura de este alcance. Mantener la barrera de NSC; seguir con trabajo independiente del objetivo.

## Investigación incremental: importación COFF larga de estilo MS (2026-11-05)

- Objetivo: producir una biblioteca de importación con objetos COFF largos, comprobar primero su enlace y ejecución mediante LLVM/LLD y después reproducir su consumo en NSC. Alcance inicial: fixtures bajo `nativesharp-interop/pruebas-proyecto/importacion-coff-larga`, artefactos en `A:/nsc-interop-build/cpp-fixtures/importacion-coff-larga` y esta bitácora; fuentes LLVM se consultan en solo lectura. Se usan los tres objetos descriptivos de LLVM y una entrada de función COFF construida con las herramientas de LLVM. No se altera todavía el compilador; se mantienen las demás restricciones generales.
- Resultado de investigación: el primer fixture combinado con tres objetos descriptivos de LLVM produjo una tabla sin la función importada; el ejecutable de control terminó con acceso inválido `0xC0000005`, por lo que no sirve como evidencia de interoperabilidad. Se sustituyó el fixture por un objeto COFF autónomo construido con `llvm-mc` y se archivó con `llvm-ar`: `llvm-dlltool -I` identificó `foo.dll`, `llvm-readobj --coff-imports` confirmó la importación de `foo`, LLD enlazó el control y este devolvió 42. NSC ON `5f7cbeac385d070d` rechazó el mismo archivo con `no supported short import entries`, confirmando la limitación real. La versión final del fixture no usa los tres objetos YAML inicialmente propuestos.
- El objeto de prueba contiene `.idata$2` y `.idata$6`; su descriptor tiene una relocalización AMD64 `ADDR32NB` en el campo Name, vinculada a un símbolo de la sección que contiene `foo.dll`. Siguiente acción: validar exhaustivamente los límites COFF y resolver esa relocalización antes de aceptar la identidad, comprobando entradas largas malformadas y preservando el comportamiento de entradas cortas y multidll. Todavía no se ha implementado la lectura larga ni se ha probado el consumidor NativeSharp. No se modificó el compilador ni se hizo commit.
- Alcance de implementación: `include/nativesharp/Linker/CoffArchiveVerifier.h`, `src/Linker/CoffArchiveVerifier.cpp`, `src/Driver/LibraryReferenceValidation.cpp`, fixtures de `pruebas-proyecto/importacion-coff-larga`, tracking y esta bitácora. Añadir lectura del descriptor largo AMD64 y resolver su relocalización de nombre con límites de sección y símbolo; conservar entradas cortas y diagnosticar estructuras no verificables. Comprobar el proyecto NS real, un descriptor alterado, `Runtime` equivocado, las regresiones de entradas cortas/multidll, ON/OFF y la suite existente. No modificar LLVM ni desplegar en C:.
- Hallazgo durante implementación: reconocer el descriptor largo y permitirlo produjo un ejecutable NS que terminó con `0xC0000005`. `llvm-readobj --coff-imports` no muestra `foo.dll` en ese PE, aunque sí aparece en el control LLD que devuelve 42. Se restituyó una barrera explícita para el formato largo sin entrada corta equivalente; aceptar el descriptor no se considera soporte. Inspeccionar además la posibilidad de archivos mixtos con importaciones largas y cortas para la misma DLL y evitar que esta barrera los deje pasar. La relación causal exacta entre la distribución de secciones y el enlace mixto todavía no se ha demostrado.
- Resultado 2026-11-05: el lector recorre secciones, tablas de símbolos y relocalizaciones acotadas para extraer el nombre de descriptores AMD64; el índice de relocalizaciones se construye en una pasada. La barrera detecta también secciones de thunks largos con relocalizaciones aunque el mismo archivo contenga entradas cortas. `importacion-coff-larga/ejecutar.cmd` verificó un descriptor con índice de símbolo inválido, el control LLD con resultado 42 y los rechazos explícitos del archivo largo y de uno mixto. Las regresiones de importación corta, multidll y caché, `pruebas-lifetime/ejecutar.cmd` y `git diff --check` pasaron; construcciones ON `bac87f653e47116a` y OFF `289e975814122b02`. En una ejecución concurrente anterior el control terminó con código de prueba 10; el ejecutable devolvió 42 al repetirlo y la batería aislada pasó, pero la causa de esa intermitencia no se estableció. El soporte de enlace largo, todos los formatos COFF y todas las entradas malformadas permanecen sin demostrar. Cambios sin commit.

## Investigación incremental: actualizar DLL y biblioteca de importación multidll (2026-11-05)

- Objetivo: extender la prueba previa para actualizar también `both.lib` de manera verificable junto con `b.dll` y NSMETA, y comprobar la invalidación y posterior reutilización de caché. Alcance: `pruebas-proyecto/CRuntimeTransitiveBActualizada.c`, `pruebas-proyecto/ejecutar-runtime-multidll.cmd`, tracking y bitácora; artefactos solo en `A:/nsc-interop-build/cpp-fixtures/multi-runtime-package`. Se conserva el resto de la prueba y las restricciones generales. No se altera el compilador.
- Resultado: la nueva DLL agrega `updated_export`, y `llvm-lib` regenera `both.lib` a partir de ambas bibliotecas de importación. La prueba confirma mediante `fc /b` que el archivo cambió; el productor vuelve a generar NSMETA. El consumidor observa una caché nueva, preserva la DLL anterior ante colisión, ejecuta la nueva con resultado 47 y después obtiene un acierto de caché. `ejecutar-runtime-multidll.cmd` terminó con código 0. Solo se probó la topología de dos DLL encadenadas y el consumidor actualizado ON; el control OFF se ejecutó antes de actualizar. Sin cambios de compilador ni commit.
- Investigación posterior de importaciones largas, sin cambios de código: `llvm/lib/ToolDrivers/llvm-dlltool/DlltoolDriver.cpp`, símbolos `identifyImportName` y `doIdentify`, identifica el nombre en `.idata$6` para bibliotecas de estilo MS y `.idata$7` sin relocaciones para las de estilo GNU. `llvm/test/tools/llvm-dlltool/Inputs/` contiene fixtures YAML que permiten estudiar ambos formatos; una identificación de nombre por sección no demuestra por sí sola que todas las entradas de un archivo mixto hayan sido inventariadas. Siguiente acción: generar fixtures verificables con las herramientas de LLVM, comprobarlos con el enlazador y diseñar la validación sin presumir equivalencia de tipos de archivo.

## Investigación incremental: actualización de DLL y metadata con caché multidll (2026-11-05)

- Objetivo: después de poblar la caché con el consumidor multidll que devuelve 42, regenerar una DLL con comportamiento distinto y volver a publicar su biblioteca NS y NSMETA; comprobar que el consumidor no restaura el resultado anterior, que el conflicto con la DLL vieja se detecta y que una ejecución con la DLL nueva devuelve 47. Alcance: `pruebas-proyecto/ejecutar-runtime-multidll.cmd`, una fuente C de fixture en el mismo directorio, tracking y esta bitácora. Los archivos creados y reemplazados están dentro de `A:/nsc-interop-build/cpp-fixtures/multi-runtime-package`; sin modificaciones al compilador ni despliegue en C:. Preservar y repetir los controles previos de identidad, DLL ausente, `both.lib` alterada y consumidor OFF.
- Resultado: `CRuntimeTransitiveBActualizada.c` cambia el resultado del fixture de 42 a 47. Se recompiló `b.dll` y se volvió a producir la biblioteca NS con NSMETA nueva en la misma ruta. La primera compilación del consumidor informó `compilation-bundle-cache: not-found` y rechazó la DLL anterior en el destino, comprobando que su contenido no se modificó; tras retirarla, la compilación volvió a informar `not-found`, publicó la nueva DLL y el ejecutable devolvió 47. La siguiente compilación informó `hit`. La batería completa pasó dos veces con código 0, incluyendo controles anteriores y ejecución OFF; `git diff --check` pasó. La prueba cambia la DLL, no la biblioteca de importación; otras topologías y actualizaciones simultáneas de `.lib` siguen sin verificar. No se modificó el compilador ni se hizo commit.

## Investigación incremental: caché y archivo de importación multidll (2026-11-05)

- Objetivo: verificar que un consumidor de biblioteca NS con un `.lib` de importación multidll detecta DLL ausentes o alteradas aun después de un acierto real de caché. Alcance: `pruebas-proyecto/ejecutar-runtime-multidll.cmd` y esta bitácora. Inspección: `configurePrecompiledModules` rechaza caché en referencias directas C++; `lookupCompilationBundle` agrega las dependencias binarias verificadas del NSMETA a las entradas antes de consultar la caché; `CompilationInputManifest::calculateIdentity` incluye `kCompilerVersion`, que contiene una identidad única de construcción. Estas observaciones requieren comprobar la ruta multidll ejecutada. Preservar las demás rutas y restricciones generales; no se modifica el compilador durante esta prueba.
- Resultado inicial: la prueba observó `compilation-bundle-cache: hit` y después rechazó `b.dll` alterada y ausente; el consumidor OFF también ejecutó con resultado 42. Extensión de la misma prueba: comprobar además que alterar `both.lib` tras el acierto no restaure el bundle. Se conserva el mismo alcance de archivos.
- Resultado final de la tanda: `ejecutar-runtime-multidll.cmd` terminó con código 0 y observó un acierto real; con esa caché poblada, rechazó `b.dll` modificada, `b.dll` ausente y `both.lib` modificada por discrepancia con NSMETA. Restauró los archivos y ejecutó el consumidor OFF con resultado 42. No se ha probado todavía en este caso una biblioteca modificada junto con su metadata actualizada, ni topologías distintas de las dos DLL encadenadas. No se alteró el compilador y no se hizo commit.

## Investigación incremental: archivo de importación con varias DLL (2026-11-05)

- Objetivo: determinar si un solo `.lib` puede importar de varias DLL y cómo representar todas sus dependencias en `CppLibraryReference` y NSMETA sin perder la validación de identidad. Alcance inicial: generación y ejecución de un fixture aislado bajo `A:/nsc-interop-build/cpp-fixtures/` con las DLL de la prueba transitiva existente y esta bitácora. No modificar todavía el compilador. Continúan las restricciones generales: Clang 23.1.0, sin Daikon, sin dirección inversa ni despliegue en C:.
- Evidencia: `llvm-lib` combinó `a.lib` y `b.lib` en `both.lib`. El proyecto `multi-runtime-investigation/Producer.nsproj` declara ese mismo archivo dos veces, con `a.dll` y `b.dll`; NSC ON `7367f210a727edf7` lo rechazó al comparar individualmente `b.dll` con `a.dll`. El catálogo NSMETA admite los dos registros y la publicación ya deduplica archivos por destino, según inspección de `LibraryMetadataVerification.cpp` y `DriverArtifactGeneration.cpp`; todavía no se ha probado el recorrido completo.
- Alcance correctivo: `src/Driver/LibraryReferenceValidation.cpp`, fixtures y scripts bajo `nativesharp-interop/pruebas-proyecto`, y esta bitácora y el tracking. Agrupar por ruta de `.lib` las DLL declaradas y verificar el conjunto completo de nombres obtenidos de sus entradas cortas, tanto para referencias directas como para cada metadata precompilada. Mantener rechazo si falta alguna DLL o si se declara una que el archivo no identifica. Comprobar productor y consumidor separados, empaquetado de ambas DLL con originales retirados, rechazo de declaración incompleta y de DLL incorrecta, y regresiones de una DLL y archivo estático sin Runtime. Bibliotecas de importación larga siguen pendientes y no se asumen estáticas.
- Resultado 2026-11-05: `validateCppImportRuntimes` agrupa por ruta de biblioteca y compara los nombres importados con los Runtime declarados; se utiliza con referencias directas y con cada catálogo NSMETA. `pruebas-proyecto/ejecutar-runtime-multidll.cmd` construyó una biblioteca de importación con entradas de dos DLL reales, rechazó una declaración faltante y otra extra, publicó el paquete NS, retiró los originales y ejecutó consumidores ON `5f7cbeac385d070d` y OFF `fbee44d91e583f97` con resultado 42. Esta prueba usa dos DLL encadenadas; aún no prueba otras topologías, importaciones largas ni todas las combinaciones de metadata y caché. ON y OFF construyeron, y pasaron las regresiones de una DLL, COFF malformado, biblioteca estática sin Runtime, `pruebas-lifetime/ejecutar.cmd` y `git diff --check`. Cambios sin commit.

## Investigación incremental: relación entre biblioteca de importación y DLL (2026-11-05)

- Objetivo: reproducir y corregir, si se confirma, una declaración `CppLibraryReference` cuya biblioteca de importación identifica una DLL distinta del archivo `Runtime`. La compilación no debe producir silenciosamente un paquete que omite la DLL requerida. Presupuesto inicial de reproducción: fixture aislado en `A:/nsc-interop-build/cpp-fixtures/static-cpp-runtime-consumer` y esta bitácora; no se altera todavía el compilador. La corrección se acotará tras identificar el punto causal y se conservarán las pruebas de las rutas actuales. Siguen vigentes las restricciones generales de este documento, incluidos Clang oficial, sin Daikon ni despliegue en C:.
- Resultado reproducido sobre NSC base `152654e5a0e2d84d4c2f245ea7590488f0497430` con cambios locales y binario ON `21c090a2af4ba425`: `WrongRuntimeProducer.nsproj` declara `api.lib` y `wrong.dll`, aunque los miembros COFF de importación de `api.lib` identifican `api.dll`. El productor y el consumidor compilaron con éxito; `llvm-readobj --coff-imports` confirmó que el ejecutable exige `api.dll`, ausente del directorio de salida. No se ejecutó el programa defectuoso. Fixture y reproducción: `A:/nsc-interop-build/cpp-fixtures/static-cpp-runtime-consumer/WrongRuntimeProducer.nsproj`, `WrongRuntimeConsumer.nsproj` y `repro-wrong-runtime.cmd`.
- Causa observada: `collectCppBinaryDependencies` (`src/Interop/Cpp/CppBinaryDependencies.cpp`) registra por separado las identidades del archivo `.lib` y la DLL declarada; `appendReferencedCppRuntimes` (`src/Driver/RuntimeClosurePackaging.cpp`) selecciona por el nombre importado y omite la DLL declarada de nombre distinto. Ninguna de esas operaciones demuestra que la pareja corresponda. El archivo de prueba contiene miembros COFF de importación corta, según `llvm-readobj --file-headers`.
- Siguiente acción: diseñar la lectura validada de nombres de DLL desde bibliotecas COFF de importación y aplicarla antes de publicar o consumir dependencias; estudiar bibliotecas con varios nombres, importación larga y entradas malformadas antes de establecer la política. La ausencia de miembros de importación corta no demuestra que una biblioteca sea estática. Aún no se ha aplicado una corrección ni verificado el caso general.
- Alcance correctivo: `CoffArchiveVerifier.h/.cpp`, `LibraryReferenceValidation.cpp`, pruebas de proyecto bajo `pruebas-proyecto/lifetime-implicito-dll` y esta bitácora. Leer de forma acotada las entradas COFF de importación corta y comparar sus nombres con `Runtime` tanto en referencias directas como en metadata; conservar el verificador normal sin cambios de conducta. Si no hay información suficiente para demostrar la pareja, emitir diagnóstico explícito en vez de publicar una salida inválida. Caso confirmado: `api.lib`/`wrong.dll`; invariante: el archivo elegido debe corresponder a la DLL nombrada por el `.lib`. Preservar las compilaciones sin Runtime, los contratos y pruebas existentes, la configuración OFF y las restricciones generales; no considerar completos los formatos de importación larga ni las bibliotecas multidll hasta comprobarlos. Verificar caso incorrecto, correcto, ausencia de cambios para `.lib` sin Runtime, consumidor con metadata existente y regresiones.
- Resultado 2026-11-05: `CoffArchiveVerifier::readShortImportDllNames` recorre miembros acotados y extrae los nombres de DLL de entradas cortas; `validateCppImportRuntime` los compara con el archivo declarado antes de usar tanto referencias C++ directas como las resueltas desde metadata. El verificador ordinario no cambia. Si no hay entradas cortas, el diagnóstico indica que la identidad no pudo establecerse; bibliotecas de importación larga y archivos con importaciones de varias DLL no quedan soportados por esta política inicial.
- Evidencia: el productor `api.lib`/`wrong.dll` fue rechazado con ambos nombres en el diagnóstico; la prueba repetible `pruebas-proyecto/lifetime-implicito-dll/ejecutar-consumo-runtime.cmd` ahora incluye ese control y el caso correcto. Una biblioteca NS con metadata incorrecta generada antes de la corrección también fue rechazada por el consumidor. En ON `7367f210a727edf7` y OFF `aac06980f6a5a480` compilaron NSC; pasaron el consumidor DLL ON/OFF con resultado 73, la cadena declarada con resultado 42, el consumidor de biblioteca C++ estática sin Runtime con resultado 73, `pruebas-lifetime/ejecutar.cmd` y `git diff --check`. El caso de metadata anterior se reprodujo con los artefactos ya existentes en el fixture, todavía no es autónomo. Pendiente: pruebas deliberadas de entradas COFF malformadas, importación larga, bibliotecas multidll y ejecución de las demás tandas. Cambios sin commit.
- Evidencia adicional: `verificar-import-malformado.py` modifica el tamaño de datos de una entrada corta real y comprueba su rechazo; está integrado en la prueba de proyecto. La primera repetición completa falló al intentar sustituir `api.source-dll` de una ejecución anterior; tras retirar ese archivo temporal específico al inicio, dos ejecuciones consecutivas de la batería terminaron con código 0. La causa exacta del rechazo al reemplazo no se determinó; la corrección es del fixture y no del compilador. Persisten otras variantes COFF malformadas por probar.

## Resultado incremental: biblioteca NativeSharp estática con DLL C++ (2026-11-05)

- Una biblioteca estática NativeSharp puede registrar y publicar una dependencia C++ formada por biblioteca de importación y DLL; el consumidor verifica sus identidades y empaqueta las DLL requeridas. Las publicaciones rechazan colisiones con contenido distinto.
- `pruebas-proyecto/lifetime-implicito-dll/ejecutar-consumo-runtime.cmd`: productor y consumidor separados ejecutaron el caso con resultado 73 al retirar los originales. Pasaron las comprobaciones de caché, DLL alterada o ausente y colisiones; también se ejecutó el consumidor con NSC construido sin integración Clang.
- `pruebas-proyecto/ejecutar-runtime-estatico-transitivo.cmd`: una cadena de dos DLL ejecutó con resultado 42 después de retirar los originales. Una DLL hermana observada pero no declarada se rechazó antes de publicar; al declararla, ambas quedaron empaquetadas. Esta protección cubre hermanos observados junto a la DLL, no identifica todas las dependencias externas posibles.
- Se corrigieron inclusiones que impedían construir NSC con la integración Clang desactivada. Compilaciones ON y OFF, batería `pruebas-lifetime/ejecutar.cmd`, consumo de dependencia C++ estática y `git diff --check`: correctos. Identidades de NSC probadas: ON `21c090a2af4ba425`, OFF `47bd` (prefijo observado).
- Pendiente: demostrar la correspondencia entre la biblioteca de importación y la DLL declarada; resolver dependencias transitivas no presentes junto a la DLL; materializar tipos C++ públicos y tratar la caché de encabezados directos. Las pruebas no demuestran interoperabilidad general ni terminan las 18 tandas. Cambios sin commit.

## Investigación incremental: construcción C++ en HIR y MIR (2026-11-05)

- Alcance: inspección del camino ya existente; no se habilita la construcción C++ en el driver ni se cambia la barrera anterior a layout. El alcance general y los archivos protegidos siguen siendo los establecidos abajo. Sin cambios de compilador ni pruebas en esta investigación; revisión comparada contra los archivos citados.
- `src/Frontend/HIR/ExpressionStatementMachine/HirConstructionFrameLowering.cpp:154`: HIR elige `ValueConstruction` para un tipo tratado como struct y conserva el ordinal del constructor seleccionado en `memberOrdinal` (líneas 180–184). Esto demuestra el flujo de datos en el código, no la ejecución de HIR con un objeto C++.
- `src/Frontend/MIR/Internal/MirNodeMachine.cpp:2041`: MIR toma para `ValueConstruction` el tamaño y la alineación del modelo de layout de valores. El acceso al tamaño C++ por `valueLayoutForTypeKey` no demuestra por sí mismo que exista un `StructValueLayout` adecuado para esa ruta; hay que verificar ambas interfaces en el camino efectivo.
- `src/LLVM/Lowering/OopLlvmConstruction.cpp:11`: el descenso de valor reserva almacenamiento, llama al constructor y después carga el valor. Para un tipo C++ no trivial esta carga y sus copias posteriores requieren analizar vida, movimiento y destrucción antes de considerarlas correctas. La ruta de clase también instala un descriptor propio de NSC (líneas 445 en adelante), por lo que no debe aplicarse automáticamente a almacenamiento C++.
- `src/Frontend/ProjectCompilation.cpp:1571`: la barrera experimental detiene el análisis antes de layout y HIR; sigue intacta. El análisis anterior no verifica firma ABI, limpieza ante excepciones ni destrucción, y no justifica eliminarla.
- Siguiente acción: inspeccionar en detalle las consultas de layout y el descenso de MIR, y diseñar una prueba aislada que compruebe la propagación del constructor sin producir código ejecutable ni alterar la ruta de producción. Registrar previamente el alcance concreto antes de modificar código o pruebas.
- Inspección adicional: `ProjectObjectLayoutModel.cpp:292–295` devuelve tamaño y alineación importados en `valueLayoutForTypeKey`; `MirNodeMachine.cpp:2052–2072`, en cambio, exige `structLayoutForType` y una función HIR asociada al constructor. La existencia de la primera consulta no satisface automáticamente estas dos exigencias. Además `layoutNamedType` rechaza expresamente tipos C++ importados (`ProjectObjectLayoutModel.cpp:617–626`). Son rutas de código observadas, no fallos reproducidos de una construcción completa: la protección impide llegar a ellas actualmente.
- Alcance del siguiente experimento: únicamente `tests/unit/CppClangSessionTests.cpp` y esta bitácora. A partir de los modelos semánticos reales ya construidos, crear un modelo de layout aislado y comprobar qué API ofrece el tamaño importado y si existe layout de struct; después intentar construir HIR aislado y registrar el resultado sin habilitar MIR, driver ni código ejecutable. Se conservarán las pruebas y barreras actuales; reconstruir el target de prueba y ejecutar la suite de lifetime. No se modifican productivos ni se compromete ninguna semántica. Revisión base y límites generales constan abajo; rango de commits sin cambios desde la línea base registrada.
- Resultado: la prueba construye un layout independiente a partir de los modelos de una expresión real `new Demo.Implicit()`. `valueLayoutForTypeKey` devuelve tamaño y alineación positivos mientras `structLayoutForType` devuelve nulo. Con ese layout, HIR aislado se construye y contiene un `ValueConstruction` que conserva los ordinales del tipo y constructor; el miembro externo asociado conserva `??0Implicit@Demo@@QEAA@XZ`. Se reconstruyó `nsc_cpp_session_tests` y pasó `call pruebas-lifetime/ejecutar.cmd` con código 0. Esto no demuestra que MIR, ABI de llamada, destrucción, copias ni excepciones funcionen. La barrera productiva permanece intacta. Cambios sin commit.
- Siguiente acción: definir y probar la representación de layout y de firma ABI para el camino MIR del constructor C++ sin reutilizar la semántica de copias de valores no triviales ni quitar la barrera; evaluar por separado el manejo de destrucción y excepciones.
- Contraste adicional de firma: `HirBuildCoordinator.cpp:938–958` asigna `builtin:void` a constructores importados, mientras el IR producido por Clang para el constructor de `ImplicitThrowing` en `A:/nsc-interop-build/cpp-fixtures/lifetime-emission/implicit-ctor.ll:9` declara retorno `ptr`. Son ejemplos del mismo ABI Windows x64, no prueba de todas las variantes de constructor. Debe modelarse y comprobarse la firma física antes de generar la llamada. `git diff --check` en NSC terminó con código 0.
- Alcance incremental 2026-11-05: `src/LLVM/Lowering/OopLlvmDeclarations.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Conservar el tipo semántico `void` de NSC y representar el retorno físico de constructor C++ como puntero en el backend Windows x64, únicamente para declaraciones externas de constructor; las funciones y constructores NS existentes deben conservar su firma. Probar mediante HIR/MIR aislados con declaración real de Clang y verificar IR, sin habilitar construcción desde el driver ni retirar la barrera de lifetime. Validar build, suite lifetime, regresiones estática/DLL y `git diff --check`. No se pretende cubrir aquí variantes con bases virtuales ni probar llamadas o destrucción. Revisión de referencia, rutas protegidas y exclusiones: las indicadas en el alcance general de este archivo.
- Resultado: `createDeclaredFunctionType` conserva el retorno semántico `void`, pero usa `ptr` como retorno físico para un constructor externo C++. Una prueba separada construye HIR y MIR a partir de declaraciones Clang reales sin construir objetos, verifica el módulo LLVM y comprueba `declare ptr` para `??0Implicit@Demo@@QEAA@XZ`; el IR observado de Clang para otro constructor en `implicit-ctor.ll` también retorna `ptr`. La construcción ON de NSC y suite pasó con contrato estático; `pruebas-lifetime/ejecutar.cmd` pasó, así como las regresiones de clase con biblioteca estática (39) y DLL (21), y `git diff --check`. Los ejemplos verificados no demuestran compatibilidad de parámetros complejos ni variantes con bases virtuales, ni ejecución de la llamada desde NativeSharp. El driver sigue bloqueando lifetime. Cambios sin commit.
- Alcance incremental: `src/Frontend/MIR/Internal/MirNodeMachine.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Incorporar en MIR el tamaño/alineación Clang para el caso de construcción C++ ya analizado, únicamente constructor predeterminado implícito sin argumentos; conservar la ruta NS y rechazar otros casos mientras no se modele su inicialización. Probar el descenso aislado de la expresión real y sus datos sin generar código ni retirar la barrera productiva. Reconstruir suite/NSC y verificar lifetime, regresiones estática/DLL y diff. El tratamiento de copias, vida, excepciones, placement y constructores explícitos sigue pendiente. Rutas protegidas y revisión de referencia: alcance general de este archivo.
- Resultado: el descenso aislado de `new Demo.Implicit()` a `ValueConstruct` consulta tamaño y alineación del tipo importado y preserva el símbolo `??0Implicit@Demo@@QEAA@XZ`; exige un constructor C++ implícito sin parámetros y mantiene la inicialización a cero de esa ruta de prueba. La suite comprobó esos datos frente al layout importado, sin generar ni ejecutar código del objeto. Reconstrucción ON con contrato estático, `pruebas-lifetime/ejecutar.cmd`, regresiones de clase estática (39) y DLL (21), y `git diff --check`: correctos. La ruta normal todavía bloquea antes de layout; constructores explícitos, semántica de valor, copias, destrucción y excepciones no están resueltos. Cambios sin commit.
- Siguiente acción: revisar y comprobar la forma de almacenamiento/valor LLVM de records importados, los efectos de copiar el resultado del constructor y el destino de destrucción; la emisión aislada no debe interpretarse como soporte seguro de lifetime completo.
- Inspección del backend: `LlvmTypeMapping.cpp:156–170` exige un layout de struct publicado en MIR para elegir el carrier LLVM; `MirLayoutImporter.cpp:474–487` importa layouts de NSC, pero todavía debe verificarse y diseñarse explícitamente la publicación de records C++. Incluso con carrier, `OopLlvmConstruction.cpp:56–60` reserva almacenamiento y su ruta de valor carga el resultado: no prueba que una copia de bytes de un C++ no trivial respete sus constructores, movimiento o destructor. La siguiente modificación debe mantener separada la representación física de esos contratos semánticos.
- Alcance incremental: `src/LLVM/Lowering/OopLlvmConstruction.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Para la construcción C++ aislada ya restringida por MIR, reservar un carrier físico de bytes del tamaño y alineación observados, sin publicar el record como struct NS ni alterar el mapeo general de parámetros/retornos. Verificar exclusivamente que el LLVM IR de la expresión real se genera y pasa el verificador con llamada al símbolo y firma de Clang. No se ejecuta el resultado, no se habilita el driver y no se considera resuelta la destrucción o copia. Reconstruir suite/NSC, ejecutar pruebas lifetime y regresiones estática/DLL; proteger alcance general y revisión base registrados abajo.
- Resultado: `lowerValueConstruction` utiliza un carrier físico `[N x i8]` para records C++ identificados por ordinal y conserva la alineación del nodo; no cambia el mapeo general de tipos. La prueba aislada de `new Demo.Implicit()` generó un módulo verificado con reserva `[4 x i8]`, declaración de constructor con retorno `ptr` y una instrucción `call` o `invoke` real a `??0Implicit@Demo@@QEAA@XZ`. Se reconstruyeron NSC y suite, pasó el contrato de runtime estático, `pruebas-lifetime/ejecutar.cmd`, regresiones de clase estática (39) y DLL (21), y `git diff --check`. No se ejecutó la construcción generada: la carga del carrier no demuestra copia válida ni limpieza/destrucción; el driver sigue bloqueado. Cambios sin commit.
- Siguiente acción: implementar y verificar las obligaciones de destrucción y limpieza de temporales C++, incluidos los casos de éxito y excepción, antes de conectar el flujo ordinario o afirmar soporte por valor.
- Alcance incremental: `src/Frontend/HIR/HirBuildCoordinator.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Preservar la identidad de destructor de los miembros C++ externos al crear su declaración HIR, con retorno semántico `void` y sin propiedad de retorno; confirmar propagación a MIR y firma LLVM de destructor para el record real `Demo::Implicit`. No cambiar la semántica de destructores precompilados NS, ni emitir limpieza o habilitar el driver. Construir NSC/suite, comprobar lifetime y regresiones estática/DLL. Referencia y restricciones de rutas: alcance general de este archivo.
- Resultado: la declaración HIR conserva `destructor` para miembros externos C++, retorno `void` y ausencia de propiedad; la prueba confirmó el mismo ordinal y símbolo `??1Implicit@Demo@@QEAA@XZ` en HIR y MIR, y la declaración LLVM `void(ptr)`. NSC ON y suite construyeron con contrato estático aprobado; pasaron `pruebas-lifetime/ejecutar.cmd`, regresiones estática (39) y DLL (21), y `git diff --check`. No se invocó este destructor desde NativeSharp ni se implementó limpieza: sigue pendiente y protegida. Cambios sin commit.
- Inspección de limpieza: `HirConstructionFrameLowering.cpp:424–432` envuelve la construcción para limpiar temporales de argumentos cuando se registra `hasOwnedTemporary`; esa condición no demuestra limpieza del propio valor C++ construido. `OwnershipQueries.cpp:105–149` identifica destructores mediante miembros del modelo, mientras `ProjectDestructionModel.cpp:93–100` genera planes de clases, no un plan C++ específico para valores importados. Se requiere comprobar el recorrido real y distinguir construcción exitosa, constructor que lanza, copia o movimiento y destrucción de temporales y locales; el símbolo presente por sí solo no genera estas obligaciones.
- Alcance incremental: `src/LLVM/Internal/OopLlvmImplementation.h`, `src/LLVM/Lowering/OopLlvmConstruction.cpp`, `src/LLVM/Lowering/OopLlvmStatements.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Implementar para una sentencia que descarte directamente un valor construido C++ la destrucción in situ después del constructor exitoso, usando el destructor externo único identificado por MIR; no cargar ni copiar el objeto descartado. Rechazar identidad ausente o ambigua. Verificar el orden y firmas en IR aislado y las regresiones existentes. No habilitar el driver, ni afirmar soporte para temporales anidados, locales, transferencia, excepciones o enlace automático: esos casos requieren integración adicional. Revisión y límites generales: los establecidos abajo.
- Resultado: para una sentencia con construcción C++ directamente descartada, LLVM usa el mismo slot del constructor para llamar al destructor externo único y evita cargar el carrier; la llamada al destructor se genera después de la llamada al constructor. La prueba aislada de `new Demo.Implicit()` verificó orden de instrucciones, firmas y ausencia de `struct.new.value`, pero no ejecutó la construcción/destrucción generada ni ejercitó la rama excepcional o las validaciones de destructor ausente/ambiguo. NSC y suite construyeron con contrato estático aprobado; pasaron `pruebas-lifetime/ejecutar.cmd`, regresiones estática (39) y DLL (21), y `git diff --check`. El driver no habilita la capacidad y todavía falta seleccionar/enlazar automáticamente el destructor implícito. Cambios sin commit.
- Siguiente acción: probar en ejecución un constructor y destructor C++ no triviales con efectos observables desde el IR generado por NSC y asegurar la selección/enlace de ambos símbolos; después ampliar a las demás formas de vida y probar explícitamente fallo de construcción y desenrollado.
- Alcance incremental de prueba: `tests/unit/CppClangSessionTests.cpp`, archivos del arnés bajo `nativesharp-interop/pruebas-lifetime/`, productos solo bajo `A:/nsc-interop-build` y esta bitácora. Agregar record C++ con miembro cuyo constructor y destructor modifican un contador; analizar construcción NativeSharp real, producir objeto NSC y cuerpos implícitos C++ desde la sesión Clang, enlazar con arnés nativo y comprobar orden y cantidad de invocaciones. La prueba aislada no habilita el driver ni pretende cubrir excepciones o uso de valores. Mantener intactas pruebas existentes, runtime y rutas protegidas según alcance general.
- Resultado: se analizó `new Demo.Observed()` desde NativeSharp y el backend de NSC emitió el objeto del consumidor; el plan seleccionó los miembros implícitos y las sesiones Clang generaron sus cuerpos. Un arnés enlazó esos tres objetos y ejecutó la función NS: el constructor de un subobjeto cambió el estado 0→1 y su destructor 1→2; el proceso devolvió 0 en O0 y O2 de generación de máquina. `llvm-nm` confirmó que el objeto NS referencia `??0Observed@Demo@@QEAA@XZ`, `??1Observed@Demo@@QEAA@XZ` y `memset`. El primer enlace detectó la referencia a `memset` y falló porque el arnés no usa CRT; se proporcionó una implementación local de prueba y se repitió exitosamente. `git diff --check`: correcto. No se ejecutó el pipeline de optimización IR de NSC ni se probó desenrollado; la selección de ambos ordinales en el arnés es explícita, no enlace automático del driver. La ruta normal continúa protegida. Cambios sin commit.
- Siguiente acción: derivar del uso real del temporal la necesidad del destructor implícito y conectarla al plan de objetos sin selección manual; probar construcción que lanza y limpieza de temporales/locales antes de habilitar la ruta general.
- Alcance incremental: `include/nativesharp/Interop/CppSemanticFunction.h`, `src/Interop/Cpp/CppSemanticFunction.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Derivar de sentencias MIR de construcción C++ directamente descartadas los ordinales de constructor y destructor, verificarlos contra los bindings y miembros del proyecto, y delegar la resolución de cuerpos implícitos al plan existente. Rechazar inconsistencias y deduplicar; no inferir obligaciones de locales, expresiones anidadas o clases no modeladas. Sustituir la selección manual del ensayo real y ejecutar O0/O2; mantener el driver protegido. Revisiones y exclusiones: alcance general de este archivo.
- Resultado: `planCppImplicitLifetimeFromDiscardedValues` recorre el MIR, contrasta cada construcción directamente descartada con su binding, exige destructor C++ único y delega al plan validado la selección de cuerpos implícitos. Dos expresiones `new Demo.Observed()` produjeron solo dos solicitudes deduplicadas; el arnés enlazó los objetos seleccionados sin lista manual y verificó en ejecución el orden constructor/destructor/constructor/destructor en O0/O2 de generación de máquina. Una prueba rechazó bindings ausentes sin modificar el plan anterior. NSC y suite construyeron con contrato estático; regresiones de clase estática (39), DLL (21) y `git diff --check` correctos. La selección y emisión se invocaron desde el arnés, no desde el driver; no cubre valores anidados, locales, excepciones ni ejecución con el pipeline de optimización IR. Cambios sin commit.
- Alcance de verificación incremental: `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, archivos `.def` nuevos en `pruebas-lifetime` y esta bitácora; productos bajo `A:/nsc-interop-build`. Utilizar un miembro cuyo constructor admita excepciones y un arnés Win32 SEH que provoque una excepción durante su construcción, capture fuera del frame NSC y compruebe que el destructor del objeto incompleto no se invoca. Repetir el caso exitoso y ambas configuraciones O0/O2 de generación. Esto prueba desenrollado SEH puntual, no captura ni compatibilidad general de excepciones C++; el driver permanece bloqueado. Preservar las pruebas existentes y las restricciones generales.
- Resultado: el miembro `CountedMember` admite fallo (`noexcept(false)`). El arnés variante invoca `RaiseException` desde su constructor y captura con `__try/__except` después de atravesar el frame de la función NSC; exige estado 1, que indica constructor iniciado y destructor no llamado. `observed-throw-O0.exe` y `observed-throw-O2.exe` devolvieron 0, y el ensayo normal con dos ciclos siguió pasando mediante `pruebas-lifetime/ejecutar.cmd`. El arnés importa `RaiseException` de Kernel32 y `__C_specific_handler` de NTDLL mediante `.def` locales. `git diff --check`: correcto. La prueba usa SEH de Windows, no `throw`/`catch` C++, no cubre destructores de subobjetos previamente construidos ni optimización de IR de NSC; tampoco activa el driver. Cambios sin commit.
- Siguiente acción: probar un constructor que falla después de construir un subobjeto para verificar que Clang limpia ese subobjeto una vez y que NSC no destruye el objeto exterior incompleto; posteriormente comprobar excepciones C++ reales y limpieza de valores vivos en NS.
- Alcance incremental de prueba: `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd` y esta bitácora; productos en `A:/nsc-interop-build`. Añadir un segundo subobjeto cuyo constructor lanza `int` después de construir el primero; enlazar un arnés de prueba con CRT estático disponible en `A:/vc2026-runtime` y capturar la excepción fuera de NSC. Verificar limpieza del primer subobjeto exactamente una vez y ausencia de destrucción del segundo y exterior, además de conservar éxito y SEH. No desplegar ni activar driver; esto no demuestra captura de excepciones dentro de NativeSharp ni limpieza de sus valores vivos. Resto de restricciones y revisión: alcance general.
- Resultado: `Observed` tiene dos miembros no triviales; el constructor del segundo ejecuta `throw 123` en la variante C++. El constructor implícito generado por Clang limpia el primer miembro; la excepción cruza la función generada por NSC y el `catch(int)` externo comprueba valor 123 y estado 3, que exige que el primer destructor corra una vez y el destructor del segundo no se invoque. El estado también detectaría una destrucción exterior que llamara a ese segundo destructor. El ensayo completo, incluido éxito, SEH y C++ con O0/O2 de generación de máquina, pasó mediante `pruebas-lifetime/ejecutar.cmd`. El primer enlace tras agregar el segundo miembro falló por `__CxxFrameHandler3`; se cambió el arnés a arranque CRT y enlace estático `libcmt`, `libvcruntime`, `libucrt` usando las bibliotecas disponibles, y se repitió. `llvm-readobj --coff-imports` mostró solo `KERNEL32.dll` para los ejecutables C++ O0/O2; `git diff --check` pasó. No se ejecutó el pipeline de optimización IR de NSC ni se probó `catch` dentro de NativeSharp, limpieza de objetos NS vivos o todos los casos de excepciones C++. Driver aún protegido; cambios sin commit.
- Siguiente acción: trasladar la planificación y emisión ya verificadas a una ruta integrada protegida, y diseñar la limpieza de objetos C++ vivos en locales y expresiones anidadas ante retornos y excepciones antes de habilitarla para proyectos ordinarios.
- Alcance incremental: `include/nativesharp/LLVM/CppLifetimeObjectEmitter.h`, `src/LLVM/CppLifetimeObjectEmitter.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Proporcionar una operación de emisión conjunta que consuma el plan validado y rutas de salida explícitas, verifique cantidad, sesiones, rutas absolutas y duplicados antes de emitir y devuelva los objetos únicamente si todos se produjeron; el llamador conserva propiedad y debe gestionar sus temporales. Reemplazar el bucle manual de emisión del ensayo y comprobar errores de entrada sin modificar la lista publicada. No conectar aún al driver ni alterar su bloqueo; probar lifetime normal, SEH y C++ en O0/O2, y build con interoperabilidad desactivada. Resto de protección y revisión: alcance general.
- Resultado: `emitCppImplicitLifetimeObjects` recibe solicitudes y destinos explícitos, valida longitud, sesión, símbolo, rutas absolutas y duplicados léxicos normalizados antes de emitir; publica la lista solamente al terminar. Si falla a mitad de emisión, los archivos parciales siguen siendo propiedad del llamador y no se eliminan automáticamente. La prueba cambió al emisor conjunto y verificó que un destino duplicado no altera la lista anterior. Construcción ON de NSC/suite con contrato estático, suite lifetime normal/SEH/C++ en O0/O2, `git diff --check` y construcción de `nsc_llvm` con interoperabilidad OFF: correctos. OFF solo verifica esa biblioteca, no un ejecutable completo. La comparación de rutas es léxica: alias de Windows por mayúsculas, enlaces o nombres equivalentes no están comprobados y deben resolverse antes de aceptar destinos arbitrarios en el driver. El driver todavía no utiliza la operación y la barrera de lifetime sigue activa. Cambios sin commit.
- Siguiente acción: resolver identidad/propiedad de destinos de objetos en el bundle temporal y conectar la operación a las rutas de ejecutable, biblioteca estática y DLL con pruebas de fallos y de publicación; no publicar sidecars ThinLTO incompletos ni levantar la barrera general de lifetime prematuramente.
- Alcance incremental: `src/LLVM/CppLifetimeObjectEmitter.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Canonicalizar destinos antes de emitir, detectar alias de Windows por diferencias de mayúsculas, rutas resueltas y archivos existentes equivalentes; rechazar errores de resolución antes de escribir. Verificar alias por mayúsculas y hardlink cuando el volumen permita crearlo, preservar la lista de resultados ante fallos y repetir el ensayo de lifetime. La creación del hardlink se limita al directorio de fixtures de la prueba y no borra archivos ajenos. No se conecta el driver ni se cambia su protección. El acceso concurrente a rutas queda por diseñar con el bundle temporal. Resto de restricciones: alcance general.
- Resultado: la validación usa `weakly_canonical`, comparación ordinal de Windows sin distinguir mayúsculas y `filesystem::equivalent` cuando ambos destinos existen; cualquier error de resolución o comparación detiene la operación antes de emitir. La suite rechazó nombres idénticos, alias por mayúsculas y hardlinks, conservando la lista anterior, y retiró los hardlinks propios del fixture. Construcción ON de NSC/suite con contrato estático, ensayos lifetime normal/SEH/C++ y `git diff --check`: correctos. La comprobación y escritura no son atómicas frente a cambios concurrentes del sistema de archivos; faltan directorios temporales de propiedad exclusiva y pruebas de enlaces simbólicos/junctions. La salida de archivos parciales al fallar una emisión sigue siendo responsabilidad del llamador. Driver aún protegido; cambios sin commit.
- Alcance incremental: `include/nativesharp/LLVM/ProjectOopObjectBundle.h`, `src/LLVM/ProjectOopObjectGenerator.cpp`, `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd` y esta bitácora. Dar al bundle la capacidad de reservar un subdirectorio nuevo propio, pedir a un emisor objetos con nombres controlados, comprobar que existen y añadirlos a `objectFiles()` solo tras éxito; en fallo no publicar rutas. El bundle conserva y limpia su propio directorio al destruirse. Rechazar la operación en bundles ThinLTO hasta diseñar bitcode y sidecars; no conectar ni activar el driver. Probar éxito, fallo sin publicación y limpieza al destruir el bundle, con `TMP`/`TEMP` apuntando a fixtures de A:, y repetir lifetime. Sin tocar directorios preexistentes ajenos; resto de restricciones y línea base: alcance general.
- Resultado: `ProjectOopObjectBundle::appendGeneratedObjects` crea un grupo nuevo dentro de su directorio temporal propio, proporciona nombres controlados al emisor y solo añade los objetos al bundle si el emisor terminó y los archivos son regulares. Rechaza bundles con indicadores ThinLTO y deja cualquier archivo parcial dentro del directorio propio para su limpieza al destruir el bundle. La suite construyó un bundle real de MIR, comprobó que un fallo de emisor no modifica la lista, agregó los cuerpos C++ mediante el emisor conjunto y verificó que sus archivos desaparecen tras destruir el bundle. `TMP`/`TEMP` del arnés apuntan a A:. NSC/suite ON con contrato estático, lifetime normal/SEH/C++, regresiones de clase estática (39) y DLL (21), y `git diff --check`: correctos. La rama de rechazo ThinLTO y fallos por archivos ausentes no se verificaron específicamente; el driver aún no llama a la interfaz y la barrera de lifetime sigue activa. Cambios sin commit.
- Siguiente acción: integrar el plan y la emisión al punto central del driver para ejecutables, bibliotecas estáticas y DLL en una ruta protegida y verificar enlace, errores y ThinLTO antes de habilitar cualquier capacidad ordinaria.
- Alcance incremental: `src/Driver/DriverArtifactGeneration.cpp`, `src/Driver/Internal/DriverInternal.h`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Conectar al generador central un auxiliar que toma MIR, bindings y sesiones vivas, deriva solicitudes para temporales C++ directamente descartados y agrega los objetos al bundle propietario antes de finalizar generación; no activar el proveedor de lifetime ni retirar barreras de compilación. Probar el auxiliar del driver con los modelos aislados reales, errores y ausencia de solicitudes; ejecutar lifetime y regresiones de ejecutable/estática/DLL con interoperabilidad ON, además de construir driver OFF. ThinLTO con solicitudes se mantiene rechazado por bundle. No se afirma todavía integración ordinaria de construcción ni soporte de ciclos de vida no modelados. Rutas y revisión: alcance general.
- Resultado: `generateProjectObjects`, punto común para las tres salidas, llama antes de completar generación a `appendCppImplicitLifetimeObjects`. Con sesiones activas el auxiliar deriva el plan de temporales directamente descartados y añade sus objetos al bundle; sin sesiones es un no-op. La suite utilizó el mismo auxiliar del driver con MIR aislado de `Observed`, verificó dos objetos propios, limpieza del bundle, rechazo de MIR no construido sin publicar objetos y no-op sin sesiones. El ensayo de ejecución normal/SEH/C++ pasó en O0/O2; NSC/suite ON con contrato estático, biblioteca `nsc_driver` OFF y `git diff --check` pasaron. Los proyectos ejecutables existentes que consumen C, clase C++ en biblioteca estática y clase C++ en DLL devolvieron 85, 39 y 21. Esas regresiones ejercitan ejecutables consumidores, no generación de una biblioteca NativeSharp estática ni DLL; tampoco se verificó específicamente el rechazo ThinLTO. La ruta ordinaria sigue sin proveedor de lifetime y bloquea la construcción, así que aún no existe una construcción C++ integrada de extremo a extremo. Cambios sin commit.
- Alcance incremental: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Agregar opción exclusivamente de prueba que permita llegar de la expresión real a los modelos HIR/MIR propios de `ProjectCompilation`, validando que cada construcción C++ sea una sentencia descartada directamente con constructor implícito sin argumentos y destructor único; rechazar cualquier otra forma antes de generar código. Mantener intacta la barrera de análisis anterior cuando no se active esta opción, y no activarla en el driver ordinario. Cambiar el ensayo observado para usar esos modelos integrados y el auxiliar del driver; verificar rechazo de forma no soportada, lifetime normal/SEH/C++ y regresiones. Mantener protegidos C:, Daikon, camino inverso y demás rutas del alcance general.
- Resultado: `enableExperimentalCppDiscardedLifetimeLoweringForTests` habilita las fases propias de `ProjectCompilation` para el ensayo; tras HIR valida que construcciones importadas solo aparezcan como sentencias descartadas con constructor implícito sin argumentos y destructor externo único. La prueba de `Observed` usa `compileProjectOopMir` directamente, el auxiliar del driver y el mismo recorrido ejecutable normal/SEH/C++ en O0/O2; pasó. Un proyecto con valor C++ en local no llegó a MIR y la opción se rechazó sin proveedor; la prueba no determinó si el local fue rechazado en semántica anterior o por la nueva validación HIR. La barrera anterior sin la opción sigue comprobada. Construcción ON con contrato estático, regresiones de ejecutables con bibliotecas estática (39) y DLL (21) y `git diff --check`: correctos. No se habilitó en driver ordinario, no se verificaron todas las formas anidadas ni se probó publicación de biblioteca o ThinLTO. Cambios sin commit.
- Verificación adicional: la suite se reconstruyó y `pruebas-lifetime/ejecutar.cmd` terminó correctamente con el caso `new Demo.Ready(1)`: la prueba exige el diagnóstico de la validación HIR sobre constructor implícito, demostrando el rechazo específico de ese constructor explícito antes de MIR. `git diff --check` pasó. Esto no demuestra el rechazo de las demás formas ni habilita la ruta ordinaria.
- Alcance incremental de verificación: `tests/unit/CppClangSessionTests.cpp` y esta bitácora; productos en `A:/nsc-interop-build`. Generar bundles reales con ThinLTO O0 y O2 desde el MIR integrado de `Observed`, invocar el auxiliar del driver y comprobar diagnóstico de rechazo y conservación de objetos. No cambiar producción ni afirmar soporte ThinLTO; preservar las exclusiones y el alcance general.
- Resultado: se generaron bundles ThinLTO reales para O0 y O2 desde el MIR de `Observed`. En ambos, `appendCppImplicitLifetimeObjects` rechazó la incorporación con el diagnóstico esperado y conservó el número previo de objetos; la suite lifetime completa terminó con código 0 tras reconstruir `nsc_cpp_session_tests`. Esto comprueba el rechazo actual, no el soporte ThinLTO ni la identidad individual de cada objeto conservado. Cambios sin commit.
- Alcance incremental de enlace: `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, nuevo `pruebas-lifetime/BundleMembers.cpp` y esta bitácora; productos solo bajo `A:/nsc-interop-build`. Enlazar con `WindowsExecutableBuilder` el bundle real de MIR y objetos de lifetime, más un objeto de miembros C++ de prueba y las bibliotecas estáticas requeridas; ejecutar el PE resultante y verificar el orden observable de dos ciclos. La prueba usa el builder real directamente, no habilita lifetime en el driver ordinario ni demuestra excepción o creación de bibliotecas NS. Mantener las restricciones generales y pruebas existentes.
- Resultado parcial: el bundle se generó y el builder invocó `C:/native-sharp/bin/nslink.exe`, pero el enlace falló con `NSL1023`: `_guard_dispatch_icall` aparece como `NODUPLICATES` tanto en un miembro de `libcmt.lib` como en uno de `libucrt.lib`. Se reprodujo mediante `call A:\llvm-project-al82dev\nativesharp-interop\pruebas-lifetime\ejecutar.cmd --bundle-link`; el PE no se generó ni ejecutó. La suite normal pasó y el caso específico queda opt-in para conservar la reproducción del fallo. En fuentes de `A:/nslink-devel` revisión `da19a58028404ea5eef467418deefce6f8fd8592`, `ArchiveResolver::resolve` recalcula definidos al inicio de cada ronda y extrae miembros durante la ronda sin refrescar ese conjunto: es una hipótesis que requiere verificar con el binario usado y los símbolos concretos, no una causa demostrada. No se modificó NSLink ni se desplegó en C:. Cambios sin commit.
- Alcance incremental de diagnóstico: conservar temporalmente en `A:/nsc-interop-build/cpp-fixtures/lifetime-emission` la respuesta y objetos del enlace fallido mediante `NSC_KEEP_LINK_TEMPS`; inspeccionar esos insumos, comparar el mismo enlace con `A:/al82dev-clang/bin/lld-link.exe` y consultar sin modificar `A:/nslink-devel`. Registrar comandos y resultados aquí. No cambiar NSLink ni despliegues, ni declarar causa hasta reproducirla. Preservar restricciones generales.
- Resultado de comparación: conservando `bundle-linked.exe.link.rsp` y los cinco objetos en `bundle-linked.exe.nslink-inputs`, se invocó `A:/al82dev-clang/bin/lld-link.exe` con esa respuesta y salida distinta `bundle-lld-comparison.exe`. El enlace devolvió 0 y el ejecutable devolvió 0 al comprobar dos ciclos. `bundle-lld-verbose.txt` registra que LLD cargó `libcmt.lib(dispatchcfg.obj)` por `_guard_dispatch_icall`, sin aparición de otro `dispatchcfg` en ese registro. NSLink 3.0.0 sigue fallando con las mismas entradas. Esto compara un caso normal, no valida excepciones ni inicialización general del CRT.
- Alcance incremental de aislamiento: archivos de prueba y script nuevos bajo `nativesharp-interop/pruebas-lifetime/archivo-duplicado/`; productos bajo `A:/nsc-interop-build/cpp-fixtures`. Generar dos bibliotecas mínimas que proveen un mismo símbolo requerido por un objeto y comparar NSLink con LLD; sin modificar sus fuentes ni instalaciones. Registrar el resultado y la revisión del binario usado. El ensayo no sustituye la prueba del runtime.
- Resultado: `archivo-duplicado/ejecutar.cmd` crea `main.obj`, que requiere `shared`, y dos bibliotecas que lo definen. Con ambas, LLD generó `lld.exe` con código 0; `C:/native-sharp/bin/nslink.exe` 3.0.0 extrajo los dos proveedores y falló con `NSL1021`, conservado en `archivo-duplicado/nslink.log`. Invocado con solo `first.lib`, NSLink generó `nslink-single.exe` con código 0. Esto demuestra extracción duplicada para este caso y concuerda con la hipótesis de conjuntos no actualizados durante una ronda en `ArchiveResolver.cpp`; no demuestra aún que el binario instalado provenga exactamente de la revisión de fuentes inspeccionada ni por qué se extrajo cada miembro del runtime. Ningún ejecutable de este caso mínimo se ejecutó. No se modificó NSLink ni C:.
- Alcance incremental de implementación experimental: `src/Frontend/ProjectCompilation.cpp`, `src/Frontend/MIR/Internal/MirNodeMachine.cpp`, `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, fixture nuevo bajo `pruebas-lifetime` y esta bitácora. Admitir, únicamente para construcción C++ directamente descartada y en la ruta experimental existente, un constructor externo no implícito con un parámetro `int` por valor y un destructor único; conservar la ruta existente de constructor implícito sin parámetros y rechazar las demás formas. Verificar HIR/MIR, firma LLVM, enlace con constructor/destructor C++ externos y ejecución con valor observable, conservando regresiones. No activar la ruta ordinaria, no cambiar NSLink, no afirmar soporte general de argumentos, objetos vivos o excepciones. Resto de restricciones: alcance general.
- Resultado: HIR y MIR admiten en el modo de prueba la construcción directamente descartada con constructor externo explícito de un `int` por valor. El plan de emisión queda vacío cuando constructor y destructor ya son externos no implícitos. Se generó y verificó LLVM, se emitieron objetos de máquina en O0/O2 y se enlazaron con implementaciones C++ externas mediante LLD; ambos ejecutables devolvieron 0 tras comprobar argumento `1`, ejecución del constructor y ejecución posterior del destructor. La suite lifetime normal, proyectos consumidores de biblioteca estática (39) y DLL (21), contrato estático de `nsc.exe` y `git diff --check` pasaron. Los niveles citados son de generación de máquina, no prueban el pipeline de optimización IR. No se ha probado el caso con NSLink, constructor que lanza, DLL de lifetime, argumentos distintos de `int` o valor almacenado; la ruta ordinaria permanece bloqueada. Cambios sin commit.
- Verificación adicional: con los mismos objetos del ensayo explícito, `C:/native-sharp/bin/nslink.exe` enlazó y ejecutó los productos O0 y O2; ambos devolvieron 0 con la comprobación de argumento y orden. Este caso usa un arnés sin CRT y una biblioteca de importación de Kernel32: no reproduce ni resuelve el conflicto al combinar `libcmt.lib` y `libucrt.lib`, ni usa el builder completo de NSC. Alcance adicional: agregar estas dos comprobaciones al script existente `pruebas-lifetime/ejecutar.cmd`, sin cambiar producción; conservar los otros ensayos.
- Alcance incremental de verificación: `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd`, fixture nuevo `pruebas-lifetime/ExplicitReadyMembers.cpp` y esta bitácora, con productos bajo `A:/nsc-interop-build`. Generar bundle real de la construcción explícita `Ready(int)`, usar el auxiliar del driver y `WindowsExecutableBuilder` con objeto de soporte C++ sin CRT, y ejecutar el PE con NSLink comprobando argumento y destructor. No habilitar el driver ordinario ni tratar como resuelto el conflicto de CRT; conservar los demás ensayos y restricciones generales.
- Resultado: el builder de NSC enlazó el bundle real de `new Demo.Ready(1)` con el objeto de miembros externos usando NSLink; el PE devolvió 0, porque el destructor verificó que el constructor recibió 1 y llamó `ExitProcess(0)`. Si el destructor no se ejecuta, `Main` retorna 17. El primer intento reveló `__CxxFrameHandler3` al declarar `ExitProcess` como potencialmente lanzadora en el fixture; tras declararla `noexcept`, el caso sin CRT enlazó y pasó. No se ejecuta código posterior al destructor en este ensayo y no demuestra desenrollado ni inicialización del CRT. Se aisló el fixture del modo compartido de snapshots; pasaron `pruebas-lifetime/ejecutar.cmd`, incluyendo LLD/NSLink O0/O2, y `pruebas-snapshot/ejecutar.cmd`. El driver ordinario sigue sin habilitar construcción y permanece sin resolver NSL1023 con CRT estático. Cambios sin commit.
- Alcance incremental de integración: `src/Driver/PrecompiledModuleConfiguration.cpp`, fixtures y script nuevos bajo `pruebas-proyecto/lifetime-explicito/` y esta bitácora. Configurar el proveedor de lifetime y la validación existente de construcciones descartadas cuando haya sesiones C++; compilar por `nsc.exe` un proyecto real que construya un objeto C++ explícito con `int`, destruya y lea un contador desde una función C++, con biblioteca estática y O0/O2. Comprobar rechazo de valor almacenado, ejecutar regresiones de clases, llamadas y snapshots, y respetar prohibición de despliegue en C:. No habilitar placement, valores vivos, tipos de argumentos no implementados ni prometer excepciones o CRT. Rutas y revisión restantes: alcance general.
- Resultado: el driver configura el proveedor de lifetime y activa la validación acotada existente al abrir sesiones C++; la API y bandera internas todavía llevan `ForTests` en su nombre. El proyecto `lifetime-explicito` usa `new Demo.Ready(1); return Demo.check();`: NSC compiló y NSLink enlazó con `ready.lib`, y los PE O0/O2 devolvieron 67 únicamente después de constructor y destructor, sin finalizar anticipadamente desde el destructor. El proyecto con local `Demo.Ready` se rechazó en análisis semántico con diagnóstico explícito de lifetime. Pasaron regresiones de clase estática y DLL en O0/O2 (39 y 21), llamadas escalares, suite lifetime, snapshots, contrato estático y `git diff --check`. Se comprobó solo el constructor externo de un `int` por valor y el destructor externo, ambos `noexcept`; no se ha verificado en el driver la emisión automática para constructores implícitos, bibliotecas NS, DLL de lifetime, excepción, otras firmas o valor almacenado. NSL1023 con CRT estático sigue pendiente. Cambios sin commit.
- Alcance incremental de verificación: fixtures y script nuevos bajo `pruebas-proyecto/lifetime-implicito/`, esta bitácora y productos bajo `A:/nsc-interop-build`. Compilar un proyecto NativeSharp que construya y descarte una clase C++ con constructor y destructor implícitos y miembro no trivial `noexcept`, enlazando implementaciones del miembro desde biblioteca estática; verificar resultado observable y emisión automática con NSC/NSLink en O0 y O2. No modificar producción sin evidencia de un fallo, no probar CRT ni valores almacenados, conservar las exclusiones generales.
- Resultado: `lifetime-implicito/ejecutar.cmd` compiló `new Demo.Outer(); return Demo.check();` desde NSC, con sesiones Clang y NSLink. `llvm-nm --defined-only` comprobó que `api.lib` no define constructor ni destructor de `Outer`; ambos PE O0/O2 enlazaron y devolvieron 73 solo si el constructor y destructor no triviales del miembro se ejecutaron en orden. La prueba verifica el camino ordinario de emisión implícita para este caso `noexcept`; no verifica excepciones, DLL, valores almacenados, RTTI, ni el conflicto del CRT estático. No hubo cambio de producción. Cambios sin commit.
- Alcance incremental de verificación: fixtures y script nuevos bajo `pruebas-proyecto/lifetime-implicito-dll/`, esta bitácora y productos bajo `A:/nsc-interop-build`. Repetir el flujo implícito con constructor/destructor del miembro y función de observación exportados desde DLL y declarados `dllimport`; generar DLL e import library con Clang/LLD y compilar consumidor con NSC/NSLink O0/O2. No modificar producción sin evidencia, ni extender a excepciones o valores almacenados. Resto de restricciones: alcance general.
- Resultado: el proyecto `lifetime-implicito-dll` compiló, enlazó y devolvió 73 en O0/O2, después de construir y destruir el miembro en el orden esperado. `llvm-readobj --coff-imports` del PE O0 confirmó `api.dll` y las importaciones `??0Member@Demo@@QEAA@XZ`, `??1Member@Demo@@QEAA@XZ` y `?check@Demo@@YAHXZ`; `--coff-exports` confirmó las mismas tres exportaciones de `api.dll`. El PE también importa `ucrtbase.dll` del sistema para soporte del runtime NSC. Esto verifica este caso `noexcept` de consumo de DLL C++, no una DLL producida por NSC, ni excepciones, objetos vivos o compatibilidad general. No hubo cambio de producción. Cambios sin commit.
- Verificación repetible adicional: el script guarda y comprueba importaciones de `api.dll` y de ambos miembros en O0 y O2 antes de ejecutar; la ejecución completa volvió a pasar.
- Alcance incremental de verificación: generar una biblioteca NativeSharp estática desde el proyecto `lifetime-implicito` en su directorio de productos bajo `A:/nsc-interop-build`, inspeccionar sus miembros, símbolos y metadatos; no modificar código de producción. Registrar si la biblioteca conserva las definiciones C++ implícitas, sin asumir que esto demuestra consumo o ejecución. Se mantienen las restricciones generales.
- Resultado preliminar: la salida de biblioteca rechazó el proyecto de ejecutable porque requiere `OutputType` de biblioteca. Alcance adicional: crear `Project-lib.nsproj` junto al fixture con `OutputType=StaticLibrary` y usarlo para la comprobación; conservar el proyecto de ejecutable.
- Resultado: con `Project-lib.nsproj`, `--emit-project-oop-lib` avanzó a generar metadata pero falló en la verificación de lectura: la API generada contiene `public pack(1) align(1) size(0) struct Member` y `Outer`, inválidos. No se publicó una biblioteca verificada ni se comprobó ejecución. Inspección: `LibraryMetadataConstruction.cpp:collectPublishedRootTypeOrdinals` reúne todos los tipos con `projectOrdinal` raíz, incluidos los C++ importados; `appendTypeLayoutMetadata` consulta los layouts NativeSharp de struct/clase y no tiene rama específica para el layout importado. Esto identifica una ruta causal candidata, no justifica todavía omitir tipos C++ que pudieran formar parte de una firma pública. No se cambió producción. Siguiente acción: distinguir referencias C++ internas de las expuestas en API y definir una representación comprobable antes de alterar metadata. Cambios sin commit.
- Investigación adicional de la biblioteca estática: `NamespaceFunctionSymbol::cppCanonicalIdentity` identifica funciones importadas, pero `appendNamespaceFunctionMetadata` no lo consulta; `ProjectNameLookup::cppImportedType` identifica los tipos importados aun cuando se les asigna el ordinal raíz. `WindowsLibraryBuilder::buildStaticObjects` archiva los objetos del proyecto y no las `extraLinkLibraries`; `appendBinaryDependencies` solo recorre `LibraryReference` con NSMETA, no `CppLibraryReference`. Por tanto, corregir únicamente `size(0)` no demuestra que un consumidor pueda recuperar `api.lib`: hay que preservar explícitamente esa dependencia o demostrar otra vía. El comentario de `WindowsLibraryBuilder.h` que dice que se aplanan bibliotecas es contradictorio con la implementación de `buildStaticObjects`; se conserva como discrepancia, sin editar silenciosamente. Aún no se ha podido verificar el consumidor porque la publicación falla antes de terminar.
- Alcance incremental: crear un script independiente `pruebas-proyecto/lifetime-implicito/ejecutar-lib.cmd` que prepare el fixture ya verificado y reproduzca la generación de biblioteca; por ahora debe fallar con el error real y no se cambia producción. Conservar las demás restricciones y registrar la salida.
- Resultado: `ejecutar-lib.cmd` reconstruyó y ejecutó primero los dos PE de referencia (73), y después reprodujo el rechazo de metadata con `size(0)` en `Member` y `Outer`; código final 1. El script queda como reproducción y no como prueba superada. La dependencia binaria C++ del consumidor y la representación de tipos expuestos siguen sin implementar ni verificar.
- Alcance incremental de investigación: fixture y script nuevos bajo `pruebas-proyecto/lifetime-implicito/` y productos en `A:/nsc-interop-build/cpp-fixtures/public-cpp-surface`; compilar una biblioteca NativeSharp cuyo struct público expone un puntero a `Demo.Outer`. Observar si alcanza metadata y qué rechaza, conservando intacto el fixture anterior. No modificar producción ni asumir que la publicación de este tipo está soportada.
- Resultado: `ejecutar-api-publica.cmd` utilizó `public struct Wrapper { public Demo.Outer* value; }` y alcanzó la misma verificación de metadata, que rechazó los `size(0)` de `Member` y `Outer`; código 1. El programa no produjo una biblioteca consumible, por lo que no demuestra reconstrucción de `Wrapper` ni preservación de su tipo apuntado. La prueba confirma que la solución debe contemplar un tipo C++ realmente referenciado por API pública, además del caso exclusivamente interno; no se modificó producción.
- Alcance incremental de investigación: conservar con `NSC_KEEP_LINK_TEMPS` los objetos del ejecutable `lifetime-implicito`, formar en `A:/nsc-interop-build` un archivo estático de prueba con los objetos de NativeSharp y cuerpos implícitos, e intentar el enlace con y sin `api.lib`. Comparar símbolos no resueltos y ejecutar el enlace exitoso; no publicar NSMETA ni modificar producción. Conservar restricciones generales.
- Resultado parcial: el proyecto volvió a ejecutarse en O0/O2 con código 73, pero al terminar no quedaron la respuesta ni el directorio de entradas en la ruta de publicación pese a `NSC_KEEP_LINK_TEMPS`; no se construyó el archivo ni se comprobó el enlace con y sin `api.lib`. El recorrido usa publicación transaccional y requiere inspeccionar dónde elimina la etapa antes de elegir otra forma de conservar los objetos; no se afirma que la opción de conservación funcione en este recorrido.
- Investigación: `ArtifactPublisher::prepareExecutable` solicita una ruta temporal a `ArtifactTransaction`, cuyo destructor elimina recursivamente `stagingRoot_`; la conservación del enlazador no impide esa limpieza exterior. `ArtifactTransaction::ensureStaging` usa `std::filesystem::temp_directory_path`, por lo que los scripts deben fijar `TMP` y `TEMP` en A: para que esa etapa no dependa del entorno heredado. Alcance correctivo: solo scripts nuevos `lifetime-explicito/ejecutar.cmd`, `lifetime-implicito/ejecutar.cmd`, `lifetime-implicito-dll/ejecutar.cmd` y `lifetime-implicito/ejecutar-api-publica.cmd`, para crear y seleccionar un subdirectorio temporal propio en sus respectivos productos A:. Repetir los tres scripts exitosos y el caso de API pública esperado fallido; no cambiar los lineamientos ni producción.
- Ampliación indispensable del mismo ajuste: `lifetime-implicito/ejecutar-lib.cmd` invoca al compilador después de que el script hijo termina su propio `setlocal`; debe fijar también su entorno temporal en A: antes de esa invocación.
- Resultado: los cinco scripts fijan `TMP` y `TEMP` dentro de su directorio de productos en A:. Las pruebas de ejecutable explícito, implícito y DLL pasaron; las reproducciones de biblioteca interna y API pública siguen fallando con el mismo `size(0)`, como se esperaba. La inspección de código demuestra que la limpieza transaccional puede borrar los temporales del enlace conservados dentro de su etapa; no se investigaron ni alteraron temporales de ejecuciones anteriores. Sin cambio de producción.
- Alcance incremental de verificación: `tests/unit/CppClangSessionTests.cpp`, `pruebas-lifetime/ejecutar.cmd` y esta bitácora; productos en `A:/nsc-interop-build`. Antes de destruir el bundle real de `Observed`, usar `WindowsLibraryBuilder::buildStaticObjects` para crear un archivo de prueba; comprobar con `llvm-nm` definiciones del constructor/destructor implícitos y referencias externas al miembro C++. Esto no publica NSMETA ni sustituye la prueba de consumidor; no modificar producción ni NSLink. Preservar restricciones generales.
- Resultado parcial: el builder creó `observed-bundle.lib` y la suite confirmó con `llvm-nm` que contiene definiciones del constructor y destructor implícitos de `Observed` y referencias sin resolver al constructor y destructor de `CountedMember`. La suite pasó; esto por sí solo aún no prueba consumo. Ampliación de la misma prueba: enlazar ese archivo O0 con el arnés C++ observable ya existente mediante LLD y ejecutarlo para comprobar el recorrido completo de los símbolos; no afirmar publicación ni consumo mediante NSMETA.
- Resultado de consumo directo: LLD enlazó `observed-bundle.lib` generado en O0 con los arneses de máquina O0 y O2, más el CRT estático usado por ese arnés; ambos ejecutables devolvieron 0 verificando dos construcciones y destrucciones. La suite completa pasó. No se verificó el archivo con optimización IR O2, ni consumo mediante NSMETA, ni el enlace de este escenario con NSLink: el conflicto NSL1023 continúa. No se modificó producción.
- Alcance incremental de verificación: nuevo fixture y script bajo `pruebas-lifetime/dependencia-binaria/`, esta bitácora y productos en `A:/nsc-interop-build/cpp-fixtures/lifetime-emission`. Separar el consumidor, `observed-bundle.lib` y los miembros C++ en otra biblioteca estática; demostrar el fallo de enlace sin esa biblioteca y la ejecución con ella usando LLD. No modificar producción, NSLink ni publicar metadata. Conservar restricciones generales.
- Resultado: `dependencia-binaria/ejecutar.cmd` produjo consumidor independiente y `dependency-members.lib`. LLD rechazó el enlace de `observed-bundle.lib` sin esa biblioteca por los constructores y destructores C++ no resueltos; al incluirla enlazó y el PE devolvió 0 después de dos ciclos verificados por el fixture, que termina mediante `ExitProcess`. El primer intento de la prueba buscaba el nombre decorado, pero LLD muestra el nombre desdecorado; se ajustó la búsqueda al diagnóstico real y se repitió correctamente. Este ensayo demuestra una dependencia binaria real y su efecto en un enlace manual, no que NSMETA ya la transporte ni compatibilidad con NSLink.
- Alcance incremental de implementación: nuevos `include/nativesharp/Interop/CppBinaryDependencies.h`, `src/Interop/Cpp/CppBinaryDependencies.cpp`, registro de fuente en `CMakeLists.txt`, prueba en `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Preparar un catálogo ordenado de referencias binarias C/C++ con nombre de archivo e identidad SHA-256 de biblioteca y runtime opcional, sin exigir NSMETA ajeno; validar archivos y detectar colisiones de nombres Windows con contenido distinto sin publicar resultados parciales. Mantener orden de enlace, no incluir headers como binarios, no modificar formato ni habilitar publicación hasta implementar lectura y resolución del consumidor. Probar huellas, colisiones, faltantes y conservación de resultado anterior; build ON y suite. Exclusiones generales intactas.
- Resultado: `collectCppBinaryDependencies` identifica bibliotecas y runtimes opcionales por nombre, tamaño y SHA-256, conserva el orden y repeticiones de enlace, omite referencias solo de header y rechaza falta de archivo, runtime sin biblioteca y colisiones de nombre Windows con contenido distinto; conserva la salida anterior ante fallo. Se probó la huella contra un valor SHA-256 calculado por separado en la prueba, runtime, repetición ordenada, colisión por mayúsculas y archivo inexistente. Construcción ON de NSC/suite con contrato estático, `pruebas-lifetime/ejecutar.cmd` y `git diff --check`: correctos. Es una pieza todavía no conectada al productor, al formato NSMETA ni al consumidor; no resuelve la publicación de biblioteca ni comprueba mutaciones concurrentes de los archivos. Cambios sin commit.
- Verificación adicional: la huella esperada para el contenido `first` se contrastó con `certutil -hashfile ... SHA256` y se fijó como valor conocido en la prueba, en lugar de calcularla con la misma implementación bajo prueba; se reconstruyó y la suite volvió a pasar. El rechazo de runtime sin biblioteca está implementado pero aún no tiene caso específico de ejecución.
- Alcance incremental de implementación: `CppBinaryDependencies.h/.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Agregar resolución para el consumidor desde un directorio absoluto: aceptar solo nombres de archivo seguros, comprobar tamaño y SHA-256 de cada biblioteca y runtime opcional, conservar el orden y no publicar rutas parciales al fallar. Probar resolución, alteración del binario, rutas de escape y conservación de salida; no integrar aún NSMETA ni afirmar publicación completa. Resto de restricciones: alcance general.
- Resultado: `resolveCppBinaryDependencies` verifica un directorio absoluto, nombres de archivo hoja, tamaño y SHA-256 de cada biblioteca y runtime antes de devolver rutas en orden; rechaza identidades de runtime incompletas y conserva la salida anterior al fallar. La suite comprobó resolución de biblioteca y runtime, repeticiones en orden, rechazo de biblioteca alterada con el mismo tamaño y rechazo de `../api.lib` sin modificar la salida. Se corrigió una advertencia de `u8path` obsoleto, se reconstruyó NSC/suite ON con contrato estático y pasó `pruebas-lifetime/ejecutar.cmd` y `git diff --check`. La resolución aún no se invoca desde NSMETA ni desde el driver; no verifica sustituciones concurrentes posteriores al hash y el caso de runtime faltante no se probó por separado. No se modificó el formato. Cambios sin commit.
- Alcance incremental de implementación: `LibraryMetadata.h`, `LibraryMetadataPayload.cpp`, `LibraryMetadataCodec.cpp`, `LibraryMetadataMerger.cpp`, `LibraryMetadata.cpp`, nuevo registro en `include/nativesharp/Interop/`, prueba dirigida en `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Definir y serializar en NSMETA 1.41 un catálogo privado ordenado de nombres, tamaños y SHA-256 de bibliotecas C/C++ y runtimes opcionales; conservar lectura/escritura de 1.40 y su proyección ABI pública. No alimentar el productor todavía y rechazar consumo con catálogo no vacío hasta conectar su resolución, para no aceptar silenciosamente una dependencia ignorada. Comprobar round-trip, orden, rechazo de downgrade con pérdida, compatibilidad 1.40 y rechazo de consumo. Mantener exclusiones generales.
- Resultado: el lector nuevo pasó la prueba de ida y vuelta del catálogo ordenado, preservación de ABI pública, lectura y escritura de 1.40 y rechazo del consumo aún no conectado; también pasaron snapshot, compilación ON y `git diff --check`. La suite independiente de tolerancia no se ejecutó porque `NSC_BUILD_TESTS=OFF`. El lector de la revisión HEAD exige `reader.atEnd()`: los archivos 1.41 agregan bytes aun con catálogo vacío, así que no puede asumirse compatibilidad con ese lector. El productor y consumidor siguen sin conectar; se rechaza fusionar catálogos distintos y no se ha verificado el binario instalado. Siguiente paso: preservar el formato anterior para bibliotecas sin catálogo e investigar publicación y consumo. Sin commit.
- Alcance incremental: `src/Interop/LibraryMetadataConstruction.cpp` y esta bitácora. Mantener el formato 1.40 en la construcción ordinaria de bibliotecas mientras el productor no publique el nuevo catálogo; el codec seguirá aceptando explícitamente 1.41 para los ensayos. Reconstruir NSC, ejecutar la prueba de snapshot y las pruebas de metadata y lifetime existentes, y comprobar la versión realmente publicada. La integración futura debe seleccionar 1.41 cuando añada dependencias C++; no conectar ahora el catálogo ni alterar archivos de C:.
- Resultado: la construcción ordinaria selecciona 1.40 y la inspección `--verify-library-metadata` del snapshot producido confirmó `format: NSMETA 1.40`. La compilación ON, el contrato estático, las suites de snapshot y lifetime y `git diff --check` pasaron. No se comprobó el archivo con un binario anterior instalado. El codec 1.41 sigue aislado y el productor deberá elevar explícitamente la versión al agregar el catálogo. Cambios sin commit.
- Análisis siguiente: `collectPublishedRootTypeOrdinals` alimenta `appendMetadataTypes`; `appendTypeLayoutMetadata` sólo reconoce layouts nativos de struct/clase, enum e interfaz y trata el resto como delegado. En cambio, `ProjectObjectLayoutModel::valueLayoutForTypeKey` sí consulta `cppImportedType` y devuelve el tamaño y alineación aportados por Clang. Por eso no basta copiar dimensiones: una API pública puede exponer `Demo.Outer*` y su identidad C++, sus declaraciones y su ciclo de vida deben sobrevivir al consumo. La representación completa y el productor/consumidor de dependencias siguen pendientes. Siguiente acción: rastrear las rutas de publicación y reconstrucción de tipos y comprobar por separado los casos privado y público antes de decidir el contrato persistente.
- Comprobación adicional: el `nsc.exe` instalado en C: verificó satisfactoriamente el snapshot 1.40 generado en A:, sin escritura en su instalación; esto verifica ese archivo concreto, no todas las versiones anteriores. Se reprodujo por separado el fallo `size(0)` tanto en la biblioteca con uso privado como en la que expone `Demo.Outer*`. `CppLibraryReference` conserva cabecera, biblioteca, runtime, modo, estándar, rutas include y macros; la apertura de sesión los necesita. El catálogo binario 1.41 por sí solo no preserva esa configuración ni las cabeceras, y un tipo reconstruido como struct NS perdería su origen y contratos C++. Falta diseñar su publicación y resolución transitiva antes de habilitar la API pública.
- Rastreo de dependencias: `ProjectGraph` conserva las referencias C++ al reconstruir proyectos fuente y `LibraryReferenceValidation.cpp` agrega sus bibliotecas al enlace; eso no serializa referencias de un proyecto ya compilado en NSMETA. `PrecompiledModuleConfiguration.cpp` rechaza la caché de artefactos para referencias C++ porque todavía no se rastrean cabeceras transitivas. El diseño del producto precompilado debe conservar configuración, identidad de cabeceras y cierre binario, sin confundirlo con la propagación existente del grafo fuente. Los dos ensayos de biblioteca siguen fallando deliberadamente; no se corrigió ni ocultó el diagnóstico.
- Alcance incremental: `CppInteropSession.h`, `ClangSession.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Exponer una copia de los nombres físicos que Clang registró en su SourceManager al analizar una unidad, ordenada y sin duplicados; probar un encabezado raíz, una inclusión transitiva y una inclusión condicional no procesada, así como el cierre de sesión. No afirmar que la lista captura comprobaciones negativas de búsqueda, archivos virtuales, cambios simultáneos, configuración completa ni identidad de contenido; no activar caché ni publicación. Reconstruir y ejecutar la suite existente. Mantener exclusiones generales.
- Resultado: `sourceFilesSnapshot` copia los nombres registrados por SourceManager, resuelve rutas relativas contra el directorio de sesión y entrega una lista ordenada y sin duplicados; devuelve `nullopt` tras cerrar, sin invalidar las copias previas. La prueba con raíz física e inclusión transitiva incluyó ambos archivos y excluyó la inclusión desactivada. Pasaron reconstrucción de NSC y suite ON, contrato estático, `pruebas-lifetime/ejecutar.cmd` y `git diff --check`. La lista por sí sola no constituye un manifiesto completo ni habilita caché, publicación o recuperación del contexto; falta registrar contenido, opciones y búsquedas negativas. Cambios sin commit.
- Alcance incremental: `CppInteropSession.h`, `ClangSession.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Agregar una consulta separada de tamaño y SHA-256 de los buffers ya cargados en SourceManager, sin reabrir archivos; rechazar la consulta completa si falta un buffer asociado y conservar el cierre de sesión. Verificar contenido de la raíz y cabecera transitiva, copia posterior al cierre y exclusión condicional. No habilitar aún caché, publicación ni afirmar protección contra modificaciones simultáneas o búsquedas negativas; reconstruir y ejecutar la suite existente.
- Resultado: `sourceFileContentsSnapshot` calcula tamaño y SHA-256 sobre buffers ya cargados por Clang, ordena por ruta y falla sin salida parcial si falta un buffer o colisionan rutas normalizadas. La prueba comparó raíz física y cabecera transitiva, ausencia de la inclusión desactivada y validez de la copia tras cerrar; compilación ON, contrato estático, `pruebas-lifetime/ejecutar.cmd` y `git diff --check` pasaron. El primer ensayo falló porque el fixture se escribió en modo texto con CRLF mientras esperaba LF; se corrigió escribiendo el fixture transitivo en modo binario y se repitió satisfactoriamente. No se simuló un buffer no disponible ni se cubren buffers virtuales, mutaciones concurrentes o búsquedas negativas; todavía no existe manifiesto completo ni se usa para caché o publicación. Cambios sin commit.
- Alcance incremental: `src/Compilation/PrecompiledSurfaceReachability.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. La reducción de metadata copia las dependencias NS pero omite el catálogo binario C++; conservarlo íntegro, incluido orden y repeticiones, cuando se reduce una superficie. Agregar prueba que fuerce una copia reducida y otra de la ruta completa; compilar la suite y verificar que el consumidor siga rechazando catálogos hasta su integración. No habilitar publicación ni consumo aún.
- Resultado: `prunedMetadata` conserva el catálogo C++ completo al reducir el módulo. La prueba forzó una superficie con un tipo alcanzado y otro excluido, verificó que se creó una copia y que se preservó la secuencia `[one.lib, two.lib, one.lib]`; otra comprobó la ruta de módulo completo. Pasaron build ON de NSC y la suite, contrato estático, `pruebas-lifetime/ejecutar.cmd` y `git diff --check`; la suite mantiene la prueba de rechazo del consumidor no conectado. Este cambio no hace consumibles aún bibliotecas con dependencias C++; faltan productor, distribución y resolución. Cambios sin commit.
- Alcance incremental: `CppBinaryDependencies.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Conectar la identidad binaria recopilada con su representación NSMETA y permitir resolver registros NSMETA mediante la validación de nombres, tamaño y SHA-256 ya existente. Comprobar ida y vuelta por el codec, orden/repeticiones, runtime opcional, detección de archivo alterado y salida intacta al fallar. No conectar todavía al driver, publicar bibliotecas ni retirar el rechazo del consumidor; compilar y ejecutar la suite existente.
- Resultado: `collectCppBinaryMetadataDependencies` transforma identidades de archivos reales en registros NSMETA y `resolveCppBinaryMetadataDependencies` usa la resolución verificada existente; ambos comprueban conversión de tamaño y no publican salida parcial si fallan. La prueba recopiló tres referencias con repetición y runtime opcional, serializó y leyó NSMETA 1.41, resolvió el catálogo en orden y rechazó una biblioteca alterada conservando la salida anterior. Pasaron compilación ON de NSC y suite, contrato estático, `pruebas-lifetime/ejecutar.cmd` y `git diff --check`. El driver aún no utiliza el puente, no distribuye archivos y el verificador sigue rechazando catálogos no vacíos. Cambios sin commit.
- Alcance incremental: `CppBinaryDependencies.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Recolectar referencias C++ de todos los proyectos fuente del grafo, en el mismo orden en que `LibraryReferenceValidation.cpp` los recorre, para construir el catálogo NSMETA; conservar repeticiones y rechazar colisiones de nombres entre proyectos sin modificar la salida. Comprobar grafo con raíz y dependencia, referencias sólo de cabecera y binarios conflictivos. No conectar todavía al productor o consumidor de metadata ni afirmar que el orden resuelve todas las dependencias de enlazadores.
- Resultado: `collectCppGraphBinaryMetadataDependencies` recorre `ProjectGraph::projects()` y las referencias declaradas de cada proyecto, delegando la identificación y detección de conflictos al catálogo existente. La prueba con dos proyectos comprobó orden `[api.lib, other.lib, api.lib]`, omisión de referencia sólo de cabecera y rechazo de una colisión entre proyectos conservando la salida previa. Build ON de NSC/suite, contrato estático, `pruebas-lifetime/ejecutar.cmd` y `git diff --check` pasaron. La prueba usa un grafo construido en memoria, no demuestra todavía publicación ni enlace final; el validador existente deduplica rutas aunque recorre los proyectos en ese orden. Cambios sin commit.
- Alcance incremental: `CMakeLists.txt`, `LibraryMetadataConstruction.cpp`, `CppClangSessionTests.cpp` si resulta necesario y esta bitácora. Colocar el recopilador de binarios sin dependencias Clang en `nsc_precompiled`, disponible también en configuración OFF, y usarlo al construir metadata de una biblioteca estática para conservar el cierre C++ del grafo y seleccionar 1.41 sólo si hay binarios. Mantener el rechazo de consumo aún no conectado, sin publicación silenciosa ni cambios en DLL; comprobar configuración ON/OFF, metadata ordinaria 1.40, suite existente y fallo controlado del proyecto con biblioteca C++. La integración de cabeceras, empaquetado y resolución final sigue pendiente.
- Resultado: el productor estático invoca el recopilador del grafo y selecciona NSMETA 1.41 cuando obtiene binarios C++; el código de catálogo se compiló en `nsc_precompiled`, tanto en ON como OFF. Build ON de NSC/suite, contrato estático, suite lifetime, snapshot y `git diff --check` pasaron; el snapshot sin C++ sigue verificándose como 1.40. La prueba real `ejecutar-lib.cmd` ejecutó sus dos EXE y luego falló con el rechazo explícito `C/C++ binary dependencies are not yet resolved by the metadata consumer`, demostrando que el catálogo llegó al verificador, no que la biblioteca se haya publicado. El fallo previo de tipos C++ `size(0)` continúa pendiente y ahora queda detrás de esa primera barrera; faltan resolución de binarios, empaquetado, cabeceras y prueba de consumo. Cambios sin commit.
- Alcance incremental: `RuntimeClosurePackaging.cpp`, `DriverInternal.h`, `DriverArtifactGeneration.cpp`, fixture nuevo bajo `pruebas-proyecto` y esta bitácora. Seleccionar DLL declaradas mediante `CppLibraryReference Runtime` cuando el PE enlazado o sus runtimes seleccionados las importen, siguiendo referencias transitivas declaradas y conservando la validación y empaquetado existente. Comprobar un ejecutable en carpeta distinta de la DLL y rechazo de colisión de nombres; bibliotecas estáticas precompiladas y su cierre NSMETA siguen fuera de esta modificación. No asumir que una DLL sin `Runtime` declarado se debe copiar. Mantener las exclusiones generales y comprobar build, pruebas existentes y `git diff --check`.
- Resultado: el driver selecciona runtimes C/C++ declarados que aparecen en importaciones del PE o de runtimes seleccionados, y los entrega al empaquetador existente tanto para EXE como para DLL. Antes de la corrección, el fixture C en carpeta separada reprodujo ausencia de DLL; después el EXE funcionó y devolvió 42. Otro EXE con clase C++ en DLL separada devolvió 73. Una DLL NativeSharp importó la DLL C empaquetada y un host llamó su método exportado con resultado 42. La primera prueba de colisión reveló que la transacción sobrescribía el destino final; se añadió preflight para runtimes C/C++ declarados y la repetición rechazó la DLL distinta conservando el fixture existente. Pasaron build ON, contrato estático, regresiones DLL C/C++, suite lifetime y `git diff --check`. No se verificaron aún selección transitiva, DLL declarada pero no usada, nombres Unicode, colisiones de DLL transitivas no declaradas ni carreras entre preflight y commit; tampoco se resuelve el cierre NSMETA. Los fixtures y resultados se encuentran en `pruebas-proyecto/ejecutar-empaquetado-runtime-c.cmd`, `pruebas-proyecto/ejecutar-empaquetado-runtime-c-dll.cmd` y `pruebas-proyecto/lifetime-implicito-dll/ejecutar-runtime.cmd`. Cambios sin commit.
- Alcance incremental: `RuntimeClosurePackaging.cpp`, `DriverInternal.h`, `DriverArtifactGeneration.cpp`, fixture nuevo bajo `pruebas-proyecto` y esta bitácora. Comprobar una cadena EXE → DLL C declarada → DLL hermana no declarada, exclusión de una DLL declarada no usada y colisión de la dependencia transitiva con el destino. En una publicación que selecciona un runtime C/C++, verificar todos los archivos realmente preparados antes de añadirlos a la transacción; no alterar el comportamiento de productos sin runtimes C/C++ seleccionados. Mantener fuera de alcance el cierre NSMETA y registrar cualquier limitación concurrente.
- Resultado: la primera ejecución de `pruebas-proyecto/ejecutar-runtime-transitivo.cmd` comprobó ejecución 42 y exclusión de la DLL no usada, pero evidenció que una DLL hermana transitiva diferente era sobrescrita en el destino. Se añadió preflight sobre todas las DLL preparadas cuando hay un runtime C/C++ seleccionado, antes de añadirlas a la transacción; la repetición rechazó la colisión y conservó el centinela. `ejecutar-runtime-transitivo-declarado.cmd` además comprobó selección de una dependencia transitiva declarada en otra carpeta y ejecución 42. Pasaron build ON, contrato estático, regresiones de empaquetado C, C++ y DLL NativeSharp con ejecución, y `git diff --check`. La comprobación de destino previa al commit aún tiene una ventana de carrera; nombres Unicode y cierre NSMETA siguen sin verificar. La protección adicional se limita a publicaciones con un runtime C/C++ seleccionado. Cambios sin commit.
- Alcance incremental: `ArtifactTransactionEntry.h`, `ArtifactTransaction.h/.cpp`, `ArtifactPublisher.h/.cpp`, `DriverInternal.h`, `RuntimeClosurePackaging.cpp`, `DriverArtifactGeneration.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Incorporar una opción por entrada que instala sin reemplazar mediante la operación atómica de Windows; usarla sólo para runtimes preparados en publicaciones con un runtime C/C++ seleccionado. Si un destino idéntico ya existe, conservarlo; si uno aparece después del registro, rechazar y revertir el resto. Probar colisión inducida después de registrar la transacción, rollback y rutas de empaquetado existentes. La observación de archivos preexistentes idénticos no evita modificaciones externas posteriores; no modificar la política de otros productos ni afirmar protección contra todos los procesos concurrentes.
- Resultado: `ArtifactTransaction::addStagedNoReplace` instala en Windows con `MoveFileExW` sin reemplazo; si aparece el destino, falla y ejecuta rollback. El publicador usa esta modalidad para DLL preparadas cuando se seleccionó un runtime C/C++; conserva sin reemplazar una DLL preexistente con contenido idéntico y mantiene por defecto la interfaz y política previas en otros productos. La prueba creó un destino después del registro y antes del commit, comprobó el error `rollback-complete`, la conservación del archivo competidor y la restauración de la salida anterior; también comprobó instalación con destino libre. Pasaron build ON, contrato estático, suite lifetime, empaquetado transitivo con recompilación sobre DLL idénticas, colisión, ejecución y prueba de DLL NativeSharp, además de `git diff --check`. La implementación alternativa no Windows no fue probada. Una DLL idéntica preexistente puede ser modificada externamente después de su comparación y el rollback general no certifica sustituciones concurrentes posteriores a la instalación; no se afirma protección completa frente a otros procesos. Cambios sin commit.
- Alcance incremental: `LibraryMetadataConstruction.cpp`, `LibraryMetadata.cpp`, fixtures de biblioteca `lifetime-implicito` y esta bitácora. Evitar que declaraciones C++ sintéticas se publiquen como declaraciones nativas propias; comprobar antes de la barrera de cierre binario que la metadata no deje tipos públicos sin resolver. Probar uso privado y API pública con `Demo.Outer*`: el privado puede alcanzar la barrera explícita de dependencias y el público debe seguir fallando, sin generar un struct NS falso ni eliminar silenciosamente el campo. Mantener bloqueado el consumo C++ hasta completar resolución, empaquetado y caché; verificar regresiones NS y DLL.
- Resultado: el productor excluye de sus declaraciones propias los tipos, funciones y estados sintéticos importados de C++; el verificador comprueba primero las referencias semánticas y conserva después el rechazo de dependencias C++ no resueltas. `verificar-metadata.cmd` confirmó que el caso privado llega a esa barrera sin el error `size(0)` y que el caso público falla señalando el campo `Wrapper.value` y `Demo.Outer`, sin publicar una API incompleta; logs en `A:/nsc-interop-build/cpp-fixtures/metadata-cpp-classification/`. Pasaron build ON, contrato estático, suite lifetime, DLL C y ejecutable con clase C++ DLL, además de `git diff --check`. El tipo público C++ sigue sin representación persistente ni consumo implementados; la biblioteca con dependencia C++ sigue sin poder publicarse hasta conectar el cierre. Cambios sin commit.
- Alcance incremental: `LibraryMetadata.cpp`, `LibraryMetadataVerification.cpp`, `LibraryReferenceValidation.cpp`, `PrecompiledModuleConfiguration.cpp`, `CppClangSessionTests.cpp`, fixture nuevo bajo `pruebas-proyecto/lifetime-implicito` y esta bitácora. Habilitar consumo de catálogos NSMETA de bibliotecas C/C++ estáticas sin runtime DLL: validar nombres/huellas, resolver cada archivo junto al NSMETA antes del enlace, conservar el orden y rechazar cambios. Bloquear caché de artefactos para estos consumidores hasta incorporar las dependencias a la identidad; seguir rechazando catálogos con DLL y APIs públicas que requieren representación C++. Probar productor y consumidor reales, ejecución, archivo faltante/modificado y caché, además de regresiones. No presentar este subcaso como cierre de la interoperabilidad.
- Resultado: el verificador acepta estructuralmente catálogos estáticos sin DLL, valida nombres y huellas y sigue rechazando runtimes DLL; el driver resuelve y verifica los `.lib` junto al NSMETA y los agrega después de la biblioteca NS sin eliminar repeticiones. Antes de la búsqueda de caché, el grafo precompilado con catálogo C++ exige `--no-artifact-cache`. `ejecutar-consumo-estatico.cmd` compiló una biblioteca NS que construye/destruye un objeto C++ privado, compiló un consumidor separado y ambos compiladores ON/OFF generaron EXE que devolvieron 73; rechazó `api.lib` alterado, ausente y caché habilitada. `verificar-metadata.cmd` confirmó publicación privada y rechazo de `Wrapper.value` público con tipo C++ no materializable. La primera suite lifetime falló porque los fixtures nuevos `.ns` quedaron incluidos por el glob de otro proyecto; se renombraron a `.ns.txt`, se ajustó su copia al directorio de prueba y la repetición aislada pasó. También pasaron build ON/OFF, contrato estático, regresiones DLL C/C++ y `git diff --check`. Los `.lib` deben estar ya junto al NSMETA: empaquetado automático, runtimes DLL en catálogos, caché y tipos C++ públicos siguen pendientes. Cambios sin commit.
- Alcance incremental: `DriverArtifactGeneration.cpp`, fixtures nuevos bajo `pruebas-proyecto/lifetime-implicito` y esta bitácora. Preparar copias de los `.lib` C/C++ del grafo en staging, comprobar tamaño y SHA-256 contra el NSMETA realmente generado y agregarlas sin reemplazo a la misma transacción del producto estático; conservar destinos idénticos y rechazar colisiones entre dependencias o con las salidas NS. Probar productor y consumidor en carpetas distintas, archivo preexistente diferente y colisión con el nombre del producto. No extender aún a DLL, cabeceras, librerías NS transitivas o caché.
- Resultado: `stageCppStaticDependencies` contrasta el catálogo del grafo con el NSMETA generado, copia cada archivo único a staging, verifica la copia por tamaño y SHA-256 y la incorpora sin reemplazo a la transacción de la biblioteca NS. Rechaza nombres que coinciden con el producto o sidecars. `ejecutar-consumo-empaquetado.cmd` confirmó rechazo de un `api.lib` distinto sin sobrescribirlo, publicación en otra carpeta, consumo y resultado 73 mientras el original estaba ausente, y rechazo del nombre `api.lib` como salida NS sin alterar la dependencia empaquetada. Pasaron build ON, contrato estático, suite lifetime, prueba de metadata pública y `git diff --check`; la prueba también ejecuta las validaciones ON/OFF y negativas existentes. No se verificaron aún nombres Unicode, ThinLTO, empaquetado de DLL ni librerías NS transitivas; la identidad de caché sigue deshabilitada para el consumidor con catálogo C++. Cambios sin commit.
- Alcance incremental: `PrecompiledModuleConfiguration.cpp`, `ejecutar-consumo-estatico.cmd` y esta bitácora. Antes de consultar la caché del consumidor, resolver y verificar cada catálogo C++ estático de las referencias NSMETA del grafo cerrado y agregar sus binarios al manifiesto de entrada; reemplazar la prohibición de caché sólo para este caso. Comprobar compilación, acierto real en la segunda ejecución, rechazo de archivo modificado o ausente aun con caché poblada y regresiones. Conservar la prohibición de caché para proyectos que importan cabeceras C++ directamente y mantener fuera de alcance DLL y tipos C++ públicos en NSMETA.
- Resultado: `lookupCompilationBundle` resuelve y verifica el catálogo de cada biblioteca NSMETA del grafo cerrado antes de buscar caché y añade sus archivos a la identidad del manifiesto. En `ejecutar-consumo-estatico.cmd`, la segunda compilación registró `compilation-bundle-cache: hit` y ejecutó con resultado 73; con caché poblada rechazó archivo alterado y ausente. Una actualización legítima del `.lib` y de NSMETA produjo `not-found` y el nuevo EXE devolvió 74; después se restauró el fixture original. Pasaron compilación ON, contrato estático, suite lifetime, empaquetado y `git diff --check`. La prueba usa una dependencia estática y no cubre modificaciones concurrentes durante la construcción; la caché de proyectos con cabeceras C++ directas sigue bloqueada y los catálogos con DLL siguen pendientes. Cambios sin commit.
- Alcance incremental: `LibraryMetadata.cpp`, `DriverArtifactGeneration.cpp`, `RuntimeClosurePackaging.cpp`, `DriverInternal.h`, `CppClangSessionTests.cpp` si corresponde, fixtures bajo `pruebas-proyecto/lifetime-implicito-dll` y esta bitácora. Permitir en NSMETA el catálogo con `.lib` de importación y DLL, verificando ambos por identidad; publicar ambos en la transacción de la biblioteca NS y seleccionar/copiar el runtime cuando el PE consumidor lo importe, incluyendo colisiones y caché. Comprobar productor y consumidor separados con la DLL original ausente, ejecución, alteración/ausencia de DLL, caché y colisión de destino. No declarar cubiertas las DLL transitivas no registradas, tipos C++ públicos ni caché de cabeceras directas.
- Corrección necesaria descubierta al verificar OFF: `PrecompiledModuleConfiguration.cpp` y `DriverArtifactGeneration.cpp` incluían la cabecera del catálogo binario dentro de `NSC_ENABLE_CPP_INTEROP`, aunque las rutas de metadata y caché también se compilan en OFF. Se añade el primer archivo al alcance para mover esas inclusiones fuera del bloque condicional; no activa Clang en OFF.
- Seguridad de publicación dentro del mismo alcance: si una DLL empaquetada importa una DLL hermana que existe junto a ella, comprobar que esa hermana esté registrada con el mismo contenido en el catálogo; si falta, rechazar la publicación en vez de producir un paquete que fallará al ejecutarse. Esto es una barrera temporal para dependencias locales no registradas, no soporte completo de descubrimiento automático. Comprobar tanto el rechazo como una cadena explícitamente registrada.
- Siguiente acción: verificar específicamente el rechazo de cada forma no descartada y la propiedad/limpieza de valores vivos ante excepciones; probar el enlace de objetos producidos por el bundle mediante las rutas reales antes de habilitar el proveedor en el driver.

## Autorización y objetivo

El usuario solicitó una tarea persistente para completar las 18 tandas de [TRACKING.md](TRACKING.md). El objetivo es implementar y verificar la utilización de bibliotecas C y C++ desde programas NativeSharp; cada casilla requiere evidencia antes de marcarse como terminada.

## Límites

- El trabajo abarca NSC en `A:/nsc-devel`, la documentación y evidencia en `A:/llvm-project-al82dev/nativesharp-interop`, y construcciones y pruebas en la unidad A:.
- Se utiliza Clang/LLVM 23.1.0. Se priorizan sus interfaces oficiales; modificar el fork de Clang exige demostrar previamente la necesidad y documentarla.
- Se permite modificar el compilador NSC en la rama de trabajo para esta interoperabilidad, preservando las compilaciones NativeSharp existentes.
- Se preservan los cambios preexistentes ajenos y las pruebas existentes. No se despliega ni instala en C:.
- Fuera de alcance: que C++ consuma NativeSharp como objetivo independiente, Daikon, .NET, vcpkg, y cambios a juegos u otros proyectos. Pasar tipos NativeSharp como argumentos a bibliotecas C++ pertenece al consumo desde NativeSharp cuando sea necesario.
- No se eliminarán directorios, cachés ni artefactos preexistentes sin comprobar su destino y su necesidad; no se publicarán cambios remotos.
- Se emplea CMD y las herramientas permitidas por los lineamientos; las rutas directas al Clang de A: están autorizadas por el usuario para evitar interferir con otros procesos.

## Punto de partida observado el 2026-11-03

- NSC: rama `development-cpp-clang`, commit `152654e5a0e2d84d4c2f245ea7590488f0497430`; `git status --short --branch` no mostró cambios locales. Es una línea base observada, no una certificación de corrección.
- Fork LLVM: rama `nativesharp-daikon-interop`, commit `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`; la carpeta `nativesharp-interop/` figura como contenido preexistente no rastreado y debe preservarse.
- `A:/al82dev-clang/bin/clang.exe --version` informa Clang 23.1.0, objetivo `x86_64-pc-windows-msvc` y el commit anterior de LLVM.
- No se evalúa todavía un rango de commits: la comparación inicial parte de las revisiones indicadas; se registrarán las revisiones posteriores al producirse.

## Secuencia, protección y aceptación

Antes de modificar un componente se identifican los archivos, símbolos, consumidores y pruebas afectados en el plan de su tanda. Los archivos no incluidos en ese análisis y los repositorios fuera de los límites permanecen protegidos. Las pruebas existentes se conservan; se agregan las necesarias para comprobar el comportamiento nuevo y las regresiones pertinentes.

La tarea termina únicamente cuando las 18 tandas y su cobertura acordada estén implementadas y verificadas con evidencia reproducible, incluidas las limitaciones descubiertas que deban resolverse. Las casillas de diagnóstico o rechazo no equivalen a interoperabilidad. Un resultado parcial no cierra el objetivo.

## Cursor inicial

- Tanda activa: 01, preparación y evidencia de partida.
- Observado: rama y revisión de NSC, rama y revisión de LLVM, versión de Clang, estado de ambos árboles.
- Avance: [línea base](LINEA-BASE-TANDAS.md) identifica ambos paquetes, el caché anterior y las opciones LLVM observadas.
- Avance: se incorporó la opción de integración de Clang en CMake, con verificaciones de configuración activada, desactivada y con paquetes de instalaciones distintas. Los detalles están en [línea base](LINEA-BASE-TANDAS.md).
- Avance: los encabezados de sesión pasaron una comprobación sintáctica aislada. El frontend `OFF` se construyó y la inyección opcional pasó la prueba de vida con sesión falsa. Se comprobó también el rechazo de una versión incompatible.
- Avance: la sesión Clang real abrió y cerró unidades C y C++ y pasó la prueba combinada que la inyecta en `ProjectCompilation`. Los resultados constan en [línea base](LINEA-BASE-TANDAS.md).
- Avance: la sesión proporciona una proyección inicial de funciones libres; símbolos C y de sobrecargas C++ se contrastaron con un objeto de Clang. No se importan todavía en NativeSharp.
- Avance: los tipos proyectados incluyen estructura de punteros y calificadores, con tamaños de objeto comprobados en Windows x64; aún no representan la clasificación ABI completa.
- Avance: la proyección inicial de records, campos y métodos pasó pruebas de layout y comparación de símbolos con objetos de Clang, incluidos destructores con y sin bases virtuales. Aún no se invocan desde NativeSharp.
- Avance: `CppLibraryReference` se carga y preserva en el grafo; la prueba dirigida ON pasó, incluyendo rechazos de compilación y caché hasta conectar las referencias. La reconstrucción OFF y la prueba de inyección pasaron. Evidencia y límites en [línea base](LINEA-BASE-TANDAS.md).
- Avance: `CppProjectSessions` abre una sesión real por referencia y conserva sus opciones; se probaron dos referencias, fallo en la segunda y cancelación. El driver y la semántica todavía no usan el puente; las protecciones siguen activas. Los diagnósticos y el aislamiento de macros se probaron adicionalmente en sesiones vivas.
- Avance: el frontend acepta y conserva conjuntos de sesiones por proyecto, verifica los propietarios y evita reemplazarlos tras iniciar la compilación. Se probaron dos proyectos reales y la construcción OFF; el driver aún no los inyecta.
- Avance: `openCppGraphSessions` abre los conjuntos a partir de un grafo y permite transferirlos al frontend. La prueba cubre dos proyectos con referencias, uno sin ellas, fallo identificado y cancelación. La invocación desde el driver permanece pendiente.
- Avance: la preparación común del driver invoca el puente cuando recibe un entorno y una factory explícitos en su contexto; prueba ON aprobada con sesiones reales. La prueba configura el contexto internamente: falta proporcionar configuración desde una invocación ordinaria y conectar semántica y enlace. Los ejecutables ON y OFF se construyeron en A: y comprobaron sintaxis de un proyecto ordinario.
- Avance: las acciones de compilador aceptan configuración explícita de recursos y headers de Clang y la prueba por consola demostró la lectura de un header de sistema. `nsc build` y `run` aceptan y reenvían las opciones; la caché aún bloquea referencias C/C++, y faltan integración semántica y enlace.
- Avance: la sesión proyecta globales de C/C++ y se contrastaron símbolos C, C++ y TLS con un objeto Clang. Aún no se importan en la semántica ni se generan accesos desde NativeSharp.
- Avance: el mapeo inicial de representaciones escalares conserva la identidad canónica de Clang y se probó con declaraciones reales; no decide ABI, sobrecargas ni habilita llamadas.
- Avance: el mapeo escalar conserva `const` y `volatile` y se probó con declaraciones Clang reales; los accesos todavía no están implementados.
- Avance: el índice semántico admite símbolos importados y un conversor transforma declaraciones libres escalares obtenidas de Clang. Las pruebas vinculan el nombre de una llamada NativeSharp y conservan dos sobrecargas C++. El driver aún no conecta la conversión ni habilita llamadas, ABI o enlace.
- Pendiente inmediato: conectar la selección bajo demanda de funciones al driver y a los sitios de uso de NSC sin perder sobrecargas aún no proyectadas; conservar el propietario estable y resolver ordinales después de `addPrecompiledProjects`. El driver configura ya el proveedor temprano de namespaces, aunque la guardia impide ejecutarlo en proyectos con referencias. Mantener la protección de compilación y caché hasta contar con semántica, ABI y enlace verificables. Completar cancelación durante trabajo y regresiones de la primera tanda; verificar el compilador efectivo antes de ejecutar CTest.
- Ninguna implementación de interoperabilidad se afirma como completada por este registro.

## Presupuesto de la primera modificación de construcción

- Repositorio y archivo: exclusivamente `A:/nsc-devel/CMakeLists.txt`.
- Símbolos y comportamiento: introducir `NSC_ENABLE_CPP_INTEROP` con valor inicial `OFF`; con `ON`, localizar Clang 23.1.0 exacto y rechazar instalaciones LLVM y Clang incompatibles. Sin habilitar la opción, conservar la búsqueda LLVM y el comportamiento existentes sin buscar Clang.
- Consumidores: configuración CMake de NSC; no se modifican rutas de ejecución, semántica del lenguaje, tests existentes ni el fork LLVM.
- Comprobaciones previstas: configurar en árboles separados de A: con opción desactivada y activada; comprobar errores con versión o instalación incompatibles cuando existan fixtures adecuados; conservar la salida y las opciones de configuración.
- La creación del target y la sesión son modificaciones posteriores de la misma tarea, sujetas a su propio desglose antes de editar.
- Para la configuración `ON`, rechazar también `NSC_HOST_LINKAGE=DYNAMIC` hasta que se compruebe su compatibilidad con los componentes estáticos de Clang; no cambiar el modo dinámico ordinario con la integración desactivada. Esta restricción está prevista en `PLAN-DETALLADO-TANDA-1.md`, sección de dependencias.

## Presupuesto de las interfaces de sesión

- Archivos nuevos de NSC: `include/nativesharp/Interop/CppSessionOptions.h`, `CppSessionDiagnostic.h`, `CppInteropSession.h` y `CppInteropSessionFactory.h`.
- Símbolos: opciones y argumentos poseídos, diagnósticos y ubicaciones poseídos, estado de sesión, contrato abstracto de cierre, cancelación poseída y resultado exclusivo de apertura.
- Efecto: proporcionar contratos para la sesión y para su inyección futura. No se crea ni ejecuta Clang, ni se alteran todavía los flujos existentes de NSC.
- Verificación: compilación dirigida de un consumidor C++20 que incluya los cuatro encabezados sin headers ni bibliotecas Clang; las pruebas funcionales de sesión se incorporan junto a la implementación real.

## Presupuesto de inyección en ProjectCompilation

- Archivos de NSC: `include/nativesharp/Frontend/ProjectCompilation.h` y `src/Frontend/ProjectCompilation.cpp`.
- Símbolos: setter y accessor de una sesión opcional, propiedad de la sesión y marca de inicio en `compileSyntaxCore`.
- Invariante: sin inyección no se abre Clang ni se cambia la semántica de la compilación. Una sesión se instala solo antes del primer intento de compilación, incluso si ese intento falla; vive hasta después de los modelos del proyecto.
- Consumidores: driver, servicio de lenguaje y clientes que crean `ProjectCompilation`; no se modifican sus rutas existentes ni se añade dependencia de enlace con Clang.
- Verificación: prueba dirigida con sesión falsa que compruebe ausencia por defecto, inyección, vida y rechazo después de comenzar; más verificación de compilación del frontend sin Clang.

## Presupuesto de la primera sesión Clang real

- Archivos nuevos: `include/nativesharp/Interop/CppClangSessionFactory.h` y `src/Interop/Cpp/ClangSession.cpp`; modificación de `CMakeLists.txt` para el target opcional y del contrato de sesión para una consulta de control del AST.
- Usar `clang::tooling::buildASTFromCodeWithArgs` y `ASTUnit`, sin ejecutar otro compilador ni modificar Clang. `ASTUnit.cpp:1419-1428` transfiere Sema y AST desde `CompilerInstance`; su disponibilidad y su vida se comprobarán, no se supondrán.
- El target `nsc_cpp_interop` existe solo en modo `ON` y enlaza `clangTooling` mediante los targets exportados. El frontend sigue sin depender de ese target.
- La sesión conserva sus opciones, diagnósticos y `ASTUnit`; valida recursos y configuración, admite entrada de archivo o buffer, y libera la unidad al cerrar. Los diagnósticos no conservan punteros a Clang. La cancelación se comprueba en puntos seguros, no mediante interrupción asíncrona.
- Pruebas previstas: C y C++ válidos; error de sintaxis con diagnóstico; rutas de recursos inexistentes; cierre repetido; cancelación; aislamiento de configuración y consulta tras cierre. Todo en A:.
- Archivo de prueba adicional: `tests/unit/CppClangSessionTests.cpp`. Una opción CMake de prueba desactivada por defecto permite compilarlo sin habilitar toda la infraestructura histórica; no cambia las pruebas existentes.
- La prueba real enlaza `nsc_frontend` y `nsc_precompiled` para comprobar que una sesión Clang real puede transferirse a `ProjectCompilation`; estos vínculos se limitan al ejecutable de prueba, sin dependencia inversa en el frontend.

## Presupuesto de extracción inicial de funciones

- Archivos: nuevo `include/nativesharp/Interop/CppFunctionDeclaration.h`; modificaciones a `CppInteropSession.h`, `ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp` y al fixture de sesión simulada en `nativesharp-interop/prueba-inyeccion-tanda1.cpp`.
- Exponer copias poseídas de declaraciones de funciones libres C/C++ identificadas en el AST, con nombre cualificado, tipos escritos y canónicos, parámetros, variadicidad, origen y símbolo calculado por Clang para declaraciones externamente visibles. La API informa explícitamente si la sesión está cerrada.
- La extracción inicial no representa métodos, templates dependientes, construcción de tipos ni llamadas desde NativeSharp; se registran como pendientes y se ampliarán en sus tandas. No considerar una firma textual una representación ABI completa.
- Verificar funciones C, namespaces y sobrecargas C++, exclusión de símbolos internos, conservación tras cierre y ausencia de acceso al AST después del cierre; contrastar los nombres externos con objetos compilados por Clang oficial.

## Presupuesto de tipos iniciales de funciones

- Archivos: nuevo `include/nativesharp/Interop/CppType.h`, más `CppFunctionDeclaration.h`, `ClangSession.cpp` y `CppClangSessionTests.cpp`.
- Adjuntar a cada retorno y parámetro un valor poseído con categoría, calificadores y, cuando Clang lo conozca, tamaño y alineación del tipo en bits. Representar la estructura de punteros, referencias y arreglos sin trasladar punteros del AST fuera de la sesión.
- Tipos no reconocidos se etiquetan explícitamente como tales. Los tamaños de objeto no sustituyen la clasificación de paso y retorno del ABI; referencias y funciones no reciben tamaños inventados.
- Comprobar escalares, punteros, calificadores, tipo incompleto y diferencias de tamaño Windows x64. No se afirman todavía llamadas NativeSharp ni cobertura completa de tipos.

## Presupuesto de proyección inicial de records

- Archivos: nuevo `include/nativesharp/Interop/CppRecordDeclaration.h`; modificaciones a `CppInteropSession.h`, `ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp` y el fixture simulado en `nativesharp-interop/prueba-inyeccion-tanda1.cpp`.
- Proyectar records nombrados en unidades y namespaces, con su estado completo, tamaño y alineación obtenidos de Clang; para definiciones completas copiar sus campos, acceso, tipos y desplazamientos en bits. Para clases C++ copiar métodos explícitos no dependientes, incluyendo constructores y destructores completos, parámetros, acceso y símbolos calculados por Clang.
- Conservar únicamente valores poseídos y distinguir una sesión cerrada. No afirmar que este paso ya importe o construya objetos desde NativeSharp. Herencia, records anónimos, instanciaciones dependientes y detalles ABI adicionales se mantienen pendientes.
- Verificar estructuras C y clases C++ completas e incompletas, offsets, accesibilidad, métodos estáticos y virtuales, y contrastar símbolos especiales con un objeto compilado por Clang.
- Extensión dentro de los mismos archivos: proyectar miembros de datos estáticos de una clase con su tipo, acceso y símbolo; se verifican con un objeto Clang. Un campo estático no se trata como parte del layout de instancia.

## Presupuesto de referencias C/C++ en proyectos

- Archivos de NSC: nuevos `include/nativesharp/Compilation/CppLibraryReference.h` y `src/Compilation/Internal/CppLibraryReferenceDescriptor.h`; modificaciones a `Project.h`, `Project.cpp`, `ProjectDescriptor.h`, `ProjectLoader.cpp`, `CompilationInputManifest.cpp`, `ProjectCompilation.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Introducir un item `CppLibraryReference` separado de `LibraryReference`, conservando el requisito `.nsmeta` de este último. Guardar header, lenguaje y estándar explícitos, biblioteca y DLL opcionales, rutas de inclusión y macros ordenadas. Admitir bibliotecas de solo headers.
- Registrar header principal, biblioteca y DLL como entradas del proyecto. Los headers transitivos todavía requieren seguimiento mediante Clang antes de habilitar cachés de compilación mixta.
- Preservar las nuevas referencias cuando `ProjectGraph.cpp` y `ProjectGraphLoader.cpp` reconstruyen objetos `Project`; esas rutas se identificaron durante el análisis y forman parte del mismo presupuesto.
- Mientras las declaraciones aún no se conectan al análisis y al enlace de NSC, la compilación de un proyecto con esta referencia falla explícitamente: aceptarla silenciosamente produciría resultados engañosos. La carga del proyecto sí se prueba independientemente.
- Verificar carga válida, errores de configuración y preservación de proyectos ordinarios; fixtures y resultados solo en A:. Este presupuesto no declara terminada la interoperabilidad.

## Presupuesto del puente de referencias a sesiones

- Archivos: nuevos `include/nativesharp/Interop/CppProjectSessions.h` y `src/Interop/Cpp/CppProjectSessions.cpp`; cambios al target opcional de `CMakeLists.txt` y `tests/unit/CppClangSessionTests.cpp`.
- Convertir las referencias ya validadas de un proyecto en opciones de sesión explícitas y abrir una sesión por referencia mediante una factory inyectada. Conservar el orden, la configuración y el índice de una falla, devolver el diagnóstico de Clang y destruir las sesiones parciales si alguna falla. El ambiente común (triple, recursos, driver y directorio de trabajo) se proporciona explícitamente.
- Probar dos referencias reales con opciones distintas, cancelación y fallo sin devolver un conjunto parcial. Este puente aún no conecta las declaraciones al análisis semántico, no activa el enlace y no retira las protecciones de compilación ni caché.

## Presupuesto de propiedad por proyecto en el frontend

- Archivos: `include/nativesharp/Interop/CppProjectSessions.h`, `src/Interop/Cpp/CppProjectSessions.cpp`, `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Asociar el conjunto de sesiones al archivo de proyecto que lo origina y permitir que `ProjectCompilation` reciba conjuntos de varios proyectos antes de comenzar. Validar rutas no vacías, propietarios únicos y sesiones listas; el frontend toma propiedad, la conserva más allá de los modelos y rechaza cambios después del inicio. Mantener compatible la inyección individual existente.
- Comprobar con sesiones Clang reales la procedencia, retención, validación y rechazo tras comenzar. La importación semántica, el driver y el enlace siguen pendientes; el rechazo de la compilación se conserva hasta implementarlos.

## Presupuesto de sesiones del grafo

- Archivos: `include/nativesharp/Interop/CppProjectSessions.h`, `src/Interop/Cpp/CppProjectSessions.cpp`, `tests/unit/CppClangSessionTests.cpp` y `CMakeLists.txt` para declarar la dependencia directa de `nsc_compilation` que usa el puente.
- Recorrer los proyectos del grafo en su orden y abrir solo los que tengan referencias C/C++, conservando proyecto y orden de referencias. Entregar todos los conjuntos o un fallo identificado por proyecto e índice; destruir los resultados parciales ante fallo o cancelación. No introducir dependencia de Clang en el grafo ni activar compilación o caché todavía.
- Probar dos proyectos reales con configuraciones distintas, transferencia del resultado a `ProjectCompilation`, fallo identificado y cancelación; usar únicamente fixtures y construcciones en A:.

## Presupuesto de composición explícita del driver

- Archivos: `src/Driver/Internal/DriverExecutionScope.h`, `src/Driver/PrecompiledModuleConfiguration.cpp`, `CMakeLists.txt` y `tests/unit/CppClangSessionTests.cpp`.
- El contexto del driver puede recibir explícitamente una factory prestada y un entorno de Clang poseído durante la ejecución. Antes de preparar metadata o consultar caché, abrir las sesiones de las referencias del grafo y transferirlas al frontend. Sin configuración explícita no se crea ninguna sesión. Solo la construcción ON enlaza el componente Clang.
- Comprobar por la ruta real de preparación del driver la inyección de sesiones reales y el diagnóstico ante fallo; conservar el rechazo temporal de compilación y caché mientras la semántica siga desconectada. No modificar el comando público ni asumir rutas de instalación.

## Presupuesto de configuración explícita desde consola

- Archivos: `src/Driver/CompilerInvocation.h`, `src/Driver/CompilerOptionParser.cpp`, `src/Driver/CompilerSession.cpp`, `src/Driver/DriverHelp.cpp` y `tests/unit/CppClangSessionTests.cpp` si se necesita cobertura adicional. No se cambian los comandos existentes ni el path global.
- Añadir `--cpp-clang-root=<ruta-absoluta>` y `--cpp-system-include=<ruta-absoluta>` repetible como configuración explícita inicial para una invocación; el root aporta el directorio de recursos `lib/clang/23` y una ruta de referencia `bin/clang.exe`, sin ejecutar ese binario. Usar el triple Windows x64 del target NSC y conservar el directorio de trabajo de la invocación. Si se proporcionan includes sin root o se usa la opción en un build OFF, informar error.
- El contexto posee la configuración y la factory durante toda la llamada síncrona. Los proyectos sin la opción conservan el flujo actual. El rechazo temporal de compilación y caché sigue activo: estas opciones no prometen todavía soporte de llamadas ni constituyen la configuración automática definitiva.
- Probar con los `nsc.exe` ON y OFF de A:, opciones válidas, rutas inválidas, combinación incompleta y proyecto ordinario; conservar el seguimiento de la salida real.

## Presupuesto de opciones C/C++ en build y run

- Archivos: `src/Driver/Internal/BuildOptions.h` y `src/Driver/ProjectCommands.cpp`; pruebas de consola en fixtures en A:.
- Reconocer y validar las dos opciones explícitas C/C++ en `build` y `run`, conservar su orden y reenviarlas a la compilación anidada. Rechazar root repetido, rutas relativas, includes inexistentes e includes sin root antes de construir.
- No retirar el rechazo de identidad de caché para proyectos con referencias C/C++, porque aún faltan semántica y dependencias transitivas. Verificar diagnóstico y transmisión usando proyectos ordinarios y/o una prueba del ejecutor, sin instalar nada en C:.

## Presupuesto de proyección inicial de variables globales

- Archivos: nuevo `include/nativesharp/Interop/CppGlobalDeclaration.h`; `include/nativesharp/Interop/CppInteropSession.h`, `src/Interop/Cpp/ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp` y `nativesharp-interop/prueba-inyeccion-tanda1.cpp`.
- Extraer declaraciones nombradas de variables de namespace o archivo desde el AST de Clang, con nombre cualificado, tipo, ubicación, enlace y nombre de símbolo cuando corresponda; deduplicar redeclaraciones. Los datos son copias poseídas. No incluir variables locales ni miembros estáticos de clase ya representados por records.
- Probar C y C++, calificadores, namespaces, enlace interno y cierre de sesión; contrastar símbolos externos con un objeto real de Clang. Esta proyección no habilita todavía lecturas ni escrituras desde NativeSharp.

## Presupuesto de mapeo escalar para declaraciones importadas

- Archivos: nuevos `include/nativesharp/Interop/CppScalarTypeMapping.h` y `src/Interop/Cpp/CppScalarTypeMapping.cpp`; `CMakeLists.txt` y `tests/unit/CppClangSessionTests.cpp`.
- Mapear exclusivamente representaciones escalares identificadas por Clang a claves `builtin:` observadas en `ProjectMemberModel` y en el backend LLVM. Conservar el tipo canónico de Clang por separado, incluso cuando dos tipos C++ distintos compartan representación; indicar explícitamente que el resultado no clasifica el ABI ni resuelve sobrecargas.
- Rechazar como no representados tipos no escalares, tamaños no admitidos y cualificadores que aún no se puedan preservar. Probar declaraciones analizadas por Clang con tamaños y signos de Windows x64 y tipos diferentes del mismo tamaño, junto con rechazos. No activar todavía su uso como soporte semántico completo ni retirar la protección de compilación.
- Ampliación: conservar explícitamente los calificadores `const` y `volatile` de valores escalares en el resultado del mapeo, sin afirmar que ya se generen accesos que respeten `volatile`. Comprobar el transporte de ambos con declaraciones Clang reales.

## Presupuesto de índice semántico inicial de funciones importadas

- Archivos: `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`, `src/Frontend/ProjectNamespaceFunctionModel.cpp`, `include/nativesharp/Frontend/NamespaceFunctionSymbol.h` y `tests/unit/CppClangSessionTests.cpp`.
- Permitir al modelo recibir antes de `build` símbolos importados ya convertidos, conservando su identidad y símbolo de Clang y el proyecto propietario; insertarlos en los mismos índices antes de enlazar referencias. Verificar y rechazar datos incompletos o identidades repetidas, sin modificar la resolución de sobrecargas ni la generación de código todavía.
- Probar el índice sobre un proyecto NativeSharp ordinario y declaraciones procedentes de una sesión Clang real. Mantener el rechazo general de proyectos C/C++ hasta conectar la conversión de declaraciones, reglas de sobrecarga, ABI y enlace; el índice aislado no certifica interoperabilidad completa.

## Presupuesto de conversión automática inicial de funciones

- Archivos: nuevos `include/nativesharp/Interop/CppSemanticFunction.h` y `src/Interop/Cpp/CppSemanticFunction.cpp`; `CMakeLists.txt` y `tests/unit/CppClangSessionTests.cpp`.
- Convertir una declaración libre proyectada por Clang a `NamespaceFunctionSymbol` con propietario explícito, namespace, firma, tipo escalar, símbolo externo e identidad C++ conservada. Devolver un fallo concreto para firmas aún no representadas; no omitirlas silenciosamente ni tratar la conversión como clasificación ABI.
- Sustituir las conversiones manuales de las pruebas C y de sobrecargas C++ por el conversor; verificar rechazos de variádicas, punteros y enlace interno. No integrar todavía la conversión en el driver ni retirar las protecciones hasta resolver cobertura, colisiones, sobrecargas, ABI y enlace.
- Evidencia: construcción `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` y ejecución de `nsc_cpp_session_tests.exe` con Clang de `A:/al82dev-clang`, ambas con salida 0 el 2026-11-04. La prueba incluye función C, dos sobrecargas C++, módulo ausente y rechazos de variádicas, punteros y enlace interno. Es una prueba aislada del índice, no una llamada ejecutable entre lenguajes; los calificadores y las reglas ABI requieren trabajo adicional.

## Presupuesto de transferencia al índice del frontend

- Archivos de NSC: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp`, `src/Frontend/ProjectCompilationSnapshots.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Añadir una entrada de símbolos ya convertidos, validada antes de comenzar, y transferirla al modelo cuando se construye. El frontend recibe datos propios de NSC y no adquiere dependencia de Clang. Rechazar modificaciones tardías y datos incompletos o repetidos.
- Verificar la conexión con un proyecto NativeSharp ordinario y una declaración obtenida de Clang. Mantener intactas las guardias de referencias C/C++, caché, ABI y enlace: esta transferencia por sí sola no habilita programas mixtos.
- Evidencia 2026-11-04: `ProjectCompilation` valida símbolos antes de empezar e instala los aceptados en su propio modelo; la prueba con `c_add` de Clang lo comprueba y verifica el rechazo de entradas incompletas, duplicadas y tardías. Construcción y prueba `nsc_cpp_session_tests ON` exitosas; construcción `nsc_frontend OFF` exitosa. El driver aún no suministra símbolos automáticamente y las guardias permanecen activas.

## Presupuesto de procedencia de declaraciones del grafo

- Archivos: nuevos `include/nativesharp/Interop/CppSemanticInventory.h` y `src/Interop/Cpp/CppSemanticInventory.cpp`; `CMakeLists.txt` y `tests/unit/CppClangSessionTests.cpp`.
- Conservar cada declaración de cada sesión con ruta propietaria e índice de referencia; resolver el ordinal y el módulo exclusivamente en el grafo suministrado después de su expansión. Registrar por declaración tanto las proyecciones posibles como las que todavía no se convierten; no omitir declaraciones, deduplicar ni habilitar compilación.
- Rechazar sesiones cerradas, propietarios ausentes o ambiguos y sesiones nulas sin devolver inventario parcial. Verificar varios proyectos, cambio de orden del grafo, procedencia, firma no representada y fallas. La incorporación al driver debe tener en cuenta que `ProjectCompilation::addPrecompiledProjects` antepone módulos y cambia ordinales.
- Evidencia 2026-11-04: el inventario se construyó y `nsc_cpp_session_tests.exe` terminó con código 0. La prueba inserta un módulo mediante `ProjectGraph::addPrecompiledProjects`, verifica ordinales posteriores para dos propietarios, retiene una función con puntero como proyección no disponible y rechaza dueños ambiguos o ausentes y sesiones nulas o cerradas. Aún no se instala este inventario en el driver; tampoco se han definido la unificación de declaraciones repetidas o la selección bajo demanda.

## Presupuesto del proveedor diferido de funciones

- Archivos de NSC: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Permitir inyectar antes de compilar un proveedor opcional, sin dependencia de Clang, que recibe el grafo después de `addPrecompiledProjects` y las sesiones del frontend. Validar los símbolos devueltos antes de instalarlos; propagar errores y rechazar configuración tardía o simultánea con símbolos ya fijados. Mantener sin cambios la ruta sin proveedor.
- Probar instalación y fallo con un proyecto ordinario, comprobando que el proveedor ve el grafo y que no se aceptan cambios tras iniciar. Las referencias C/C++ continúan bloqueadas hasta resolver semántica, ABI, caché y enlace; esta prueba no demuestra llamadas mixtas.
- Evidencia 2026-11-04: `nsc_cpp_session_tests` ON y `nsc_frontend` OFF construidos correctamente; prueba ON con código 0. Se inyectó una sesión Clang real en un proyecto ordinario y el proveedor pobló su índice después de preparar el grafo. También se probaron error del proveedor, salida inválida, configuración contradictoria en ambos órdenes y rechazo tardío. `git diff --check` pasó. El driver aún no configura el proveedor y los proyectos con referencias C/C++ siguen rechazándose.

## Presupuesto de firma completa y enlace de funciones

- Archivos: `include/nativesharp/Interop/CppFunctionDeclaration.h`, `src/Interop/Cpp/ClangSession.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Copiar desde Clang el tipo canónico completo de función, el nombre de la convención de llamada y el enlace de lenguaje C/C++. Conservarlos como información de identificación; no presentarlos como clasificación ABI ni utilizar el texto como demostración suficiente de equivalencia entre unidades.
- Probar en sesiones C y C++ con función ordinaria, `extern "C"`, `noexcept` y `__vectorcall`; mantener las guardias de compilación y enlace.
- Evidencia 2026-11-04: compilación de `nsc_cpp_session_tests ON` y ejecución con Clang de A: terminaron con código 0. Las comprobaciones incluyen enlace C, enlace C++, tipo `noexcept` y `vectorcall`. Estos campos aún no certifican la compatibilidad ABI.

## Presupuesto de agrupación de funciones del inventario

- Archivos: nuevos `include/nativesharp/Interop/CppSemanticFunctionGroups.h` y `src/Interop/Cpp/CppSemanticFunctionGroups.cpp`; `CMakeLists.txt` y `tests/unit/CppClangSessionTests.cpp`.
- Agrupar únicamente proyecciones escalares con el mismo propietario, nombre cualificado y símbolo externo cuando además coincidan tipo canónico completo, convención de llamada y enlace de lenguaje; conservar los índices de todas sus procedencias. Detectar conflicto cuando esa clave represente firmas distintas. Mantener por separado las entradas no proyectadas y las sobrecargas con símbolos distintos; no inferir equivalencia ODR ni disponibilidad del símbolo en una biblioteca.
- Verificar declaraciones idénticas en sesiones distintas, firmas incompatibles con el mismo símbolo C, propietarios y sobrecargas distintos, además de entradas no proyectadas. La agrupación todavía no habilita el driver ni las llamadas.
- Evidencia 2026-11-04: `nsc_cpp_session_tests ON` se construyó y ejecutó con código 0; las declaraciones C idénticas en dos sesiones conservan ambos índices de referencia, el mismo símbolo en otro proyecto queda separado, dos sobrecargas C++ permanecen separadas, una firma con puntero permanece no proyectada y dos sesiones con retornos `int` y `double` para el mismo símbolo C producen conflicto. Se conserva y compara el triple. `git diff --check` pasó. La detección de conflictos sólo abarca proyecciones escalares: no demuestra equivalencia completa entre declaraciones ni detecta todavía conflictos donde alguna de las firmas no se proyecta.
- Ampliación indispensable: conservar también el ordinal en cada entrada no proyectada y comprobar conflicto de tipo canónico, convención, enlace y triple para todas las declaraciones externas con símbolo, incluso si una firma contiene punteros o aún carece de traducción. Probar dos sesiones reales con igual símbolo C y parámetros incompatibles, uno de ellos no proyectable. La coincidencia sigue siendo sólo candidata, no prueba ODR ni ABI.
- Evidencia de la ampliación, 2026-11-04: reconstrucción ON, ejecución de `nsc_cpp_session_tests.exe` y `git diff --check` terminaron con código 0. Se contrastaron `int pointer_value(int*)` e `int pointer_value(int)` desde sesiones Clang distintas. El conflicto se comprueba por propietario y símbolo externo aun si la firma no se proyecta; las entradas no proyectadas no se agrupan ni se hacen utilizables. Esta evidencia sustituye la limitación de detección descrita en el punto anterior, no la limitación ODR/ABI.

## Presupuesto de selección de funciones por nombre

- Archivos: `include/nativesharp/Interop/CppSemanticFunctionGroups.h`, `src/Interop/Cpp/CppSemanticFunctionGroups.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Seleccionar candidatos por ordinal propietario y nombre C++ cualificado; conservar sobrecargas y firmas no proyectadas con índices al inventario original. Incluir en la comprobación de conflictos otras declaraciones del mismo símbolo externo dentro del propietario, aunque usen otro nombre cualificado. Un conflicto ajeno al nombre y símbolos consultados no bloquea esa consulta.
- Verificar nombre ausente, sobrecargas, firma no proyectada, propietarios distintos, conflicto relacionado y conflicto ajeno usando sesiones Clang reales. Esta consulta no resuelve sobrecargas ni habilita llamadas.
- Evidencia 2026-11-04: construcción `nsc_cpp_session_tests ON`, ejecución con `A:/al82dev-clang` y `git diff --check` terminaron con código 0. Las pruebas incluyen selección por dueño y nombre, nombre ausente, dos sobrecargas escalares más una con puntero, índices originales, conflicto ajeno y conflicto de `extern "C"` entre `alpha::alias` y `beta::alias`. Esta API aún no está conectada al analizador semántico ni clasifica ABI.

## Presupuesto de disponibilidad de sintaxis al proveedor

- Archivos: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp`, `src/Frontend/ProjectCompilationSnapshots.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Invocar el proveedor de funciones inmediatamente antes de construir el índice de funciones, después de parsear unidades y construir nombres/miembros; pasarle vistas de las unidades y del grafo definitivo. Propagar y validar resultados como antes. Sin proveedor se mantiene el flujo previo. El rechazo general de referencias mixtas permanece hasta la integración completa.
- Verificar que el proveedor recibe sintaxis real, se ejecuta una vez en la fase de miembros, preserva su fallo y conserva el comportamiento con el proveedor ausente. No afirmar que ya se extraigan automáticamente los nombres demandados.
- Evidencia 2026-11-04: `nsc_cpp_session_tests ON` se construyó y ejecutó con código 0; el proveedor recibió unidades con árbol sintáctico, se invocó una vez en miembros y no se invocó en `compileProjectSyntax`. Se mantuvieron las pruebas de errores y símbolos inválidos. La construcción `nsc_frontend OFF` y `git diff --check` también terminaron con código 0. El proveedor del driver y la extracción de nombres demandados permanecen pendientes.

## Presupuesto de namespaces de funciones importadas

- Entrada y fallo a verificar: una función `tools::blend` proyectada de Clang está en el índice, pero la expresión NativeSharp `tools.blend(...)` no la encuentra porque `ProjectNamespaceFunctionModel::namespaceExists` sólo consulta namespaces de `ProjectNameLookup`.
- Archivos: `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`, `src/Frontend/ProjectNamespaceFunctionModel.cpp` y `tests/unit/CppClangSessionTests.cpp`. Indexar los namespaces y ancestros de funciones C/C++ ya instaladas, respetando visibilidad y propietario; la compilación sin símbolos importados permanece igual.
- Probar llamada cualificada en sintaxis NativeSharp real y dos sobrecargas extraídas de Clang. Conservar el rechazo de compilación mixta y no presentar el vínculo semántico como una llamada ABI ejecutable.
- Evidencia 2026-11-04: antes del cambio `nsc_cpp_session_tests.exe` terminó con código 1 en `qualified NativeSharp call finds the imported C++ namespace and overloads`. Después de indexar los namespaces importados, la construcción ON y la ejecución terminaron con código 0: `tools.blend(1)` encuentra dos candidatos y `root.sub.foo(1)` encuentra uno, a partir de declaraciones Clang. La construcción `nsc_frontend OFF` y `git diff --check` terminaron con código 0. No se han probado alias, directivas using ni visibilidad entre proyectos y este resultado no acredita ejecución ni ABI.

## Presupuesto de namespaces importados en la fase de nombres

- Entrada: símbolos C++ ya proyectados e inyectados antes de la compilación; consumidores `using tools;` y `using T = tools;` de NativeSharp. `ProjectNameLookup` valida estas directivas antes de construir `ProjectNamespaceFunctionModel`.
- Archivos: `include/nativesharp/Frontend/ProjectNameLookup.h`, `src/Frontend/ProjectNameLookup.cpp`, `src/Frontend/NameLookup/QualifiedNamespaces.cpp`, `src/Frontend/ProjectCompilation.cpp` y `tests/unit/CppClangSessionTests.cpp`. Registrar los namespaces propietarios de funciones inyectadas antes de enlazar nombres, sin generar declaraciones NativeSharp ni cambiar el comportamiento sin símbolos importados. Se utiliza el separador de nombres ya existente en `QualifiedNamespaces.cpp`.
- Comprobar directiva y alias sobre un proyecto NativeSharp que recibe sobrecargas reales de Clang. Los namespaces disponibles sólo mediante el proveedor tardío y los namespaces sin funciones requieren integración posterior; mantener intactas las guardias de compilación mixta.
- Evidencia 2026-11-04: antes del cambio la prueba dirigida devolvió código 1 con dos fallos de using/alias; después, la construcción ON y `nsc_cpp_session_tests.exe` devolvieron código 0 para `using tools;` y `using T = tools;`, con ambas sobrecargas Clang. También se comprobó rechazo de propietario incorrecto y de namespace inexistente en un proyecto ordinario. La construcción `nsc_frontend OFF` y `git diff --check` devolvieron código 0. Estos casos usan inyección directa; la recolección temprana automática desde sesiones sigue pendiente.

## Presupuesto de extracción de namespaces Clang

- Archivos: nuevo `include/nativesharp/Interop/CppNamespaceDeclaration.h`; `include/nativesharp/Interop/CppInteropSession.h`, `src/Interop/Cpp/ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp` y `nativesharp-interop/prueba-inyeccion-tanda1.cpp`.
- Extraer copias de declaraciones de namespace directamente del AST, incluyendo vacíos, anidados, inline y anónimos; indicar si están dentro de uno anónimo y conservar ubicación. Evitar repetir redeclaraciones, pero recorrer cada reapertura para no perder hijos. Una sesión cerrada no expone el AST y los datos copiados siguen siendo válidos.
- Verificar el resultado con sesiones C/C++ reales, namespace reabierto y sus hijos, namespaces anónimos y cierre. Esta extracción no decide todavía visibilidad NativeSharp ni activa compilación mixta.
- Evidencia 2026-11-04: `nsc_cpp_session_tests ON`, su ejecución, la construcción `nsc_frontend OFF`, `verificar-inyeccion-tanda1.cmd` y `git diff --check` terminaron con código 0. La prueba cubre namespaces vacíos, reabiertos con hijos distintos, anidados, inline, anónimos, ubicación y vida posterior al cierre.

## Presupuesto del inventario de namespaces por proyecto

- Archivos: nuevos `include/nativesharp/Interop/CppNamespaceInventory.h`, `src/Interop/Cpp/CppNamespaceInventory.cpp`; `CMakeLists.txt` y `tests/unit/CppClangSessionTests.cpp`.
- Asociar cada declaración a su archivo propietario, índice de referencia y ordinal del grafo final; conservar inline, anonimato y ubicación. Un dueño ausente o ambiguo, una sesión nula o cerrada producen un fallo identificado y ningún resultado parcial. No trasladar namespaces anónimos al espacio de nombres visible ni alterar todavía el driver.
- Probar dos proyectos, varias referencias y un grafo con módulos precompilados; comprobar información de anónimos y rechazos con sesiones Clang reales.
- Evidencia 2026-11-04: construcción ON y ejecución de `nsc_cpp_session_tests.exe` devolvieron código 0. Se comprobó procedencia tras insertar un módulo precompilado, referencias y dueños diferentes, anonimato, propietario ausente o ambiguo y sesiones nulas o cerradas. Aún no se registran automáticamente namespaces en el frontend.

## Presupuesto del proveedor temprano de namespaces

- Archivos: `include/nativesharp/Interop/CppNamespaceInventory.h`, `src/Interop/Cpp/CppNamespaceInventory.cpp`, `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Convertir nombres visibles del inventario a segmentos de namespace NativeSharp, deduplicando por propietario y conservando el inventario original; excluir anónimos y descendientes de anónimos. Instalar un proveedor opcional antes de la fase de nombres, sobre el grafo definitivo y las sesiones ya inyectadas; sin proveedor, conservar el comportamiento anterior. Las reglas de búsqueda de namespaces inline y alias C++ quedan pendientes.
- Probar con Clang real `using` y alias sin inyección manual de namespaces, comprobando propietarios, anónimos, fallas, propiedad de sesiones y ausencia de cambios en compilación ordinaria. Mantener las guardias de compilación mixta y caché.
- Evidencia 2026-11-04: construcción ON, `nsc_cpp_session_tests.exe` y construcción `nsc_frontend OFF` terminaron con código 0. Un proyecto NativeSharp ordinario con sesiones Clang inyectadas resuelve `using tools;` y `using T = tools;` mediante proveedores de namespaces y funciones; se comprueban exclusión de anónimos, deduplicación, grafo expandido y fallo del proveedor. No es todavía una invocación ordinaria del driver ni una ejecución mixta.

## Presupuesto de conexión del proveedor de namespaces al driver

- Archivos: `src/Driver/PrecompiledModuleConfiguration.cpp` y `tests/unit/CppClangSessionTests.cpp` si se requiere ampliar la prueba existente.
- Cuando el driver abra sesiones de referencias C/C++, configurar el proveedor temprano para inventariar namespaces sobre el grafo final y entregar los nombres visibles; incluir proyecto y referencia en errores. No activar el proveedor para proyectos sin sesiones ni enlazar Clang en la construcción OFF.
- Construir el driver ON y OFF y comprobar la preparación existente. El rechazo temporal de compilación de proyectos con referencias permanece activo, por lo que no se afirmará ejecución de esta ruta por invocación ordinaria hasta retirar las guardias con ABI y enlace comprobados.
- Evidencia 2026-11-04: construcción y prueba `nsc_cpp_session_tests ON`, construcción `nsc_driver OFF` y `git diff --check` devolvieron código 0. La prueba del driver comprueba la apertura y retención de sesiones; el proveedor equivalente se ejecuta en prueba mediante inyección explícita con Clang real. La configuración del proveedor por el driver está implementada, pero su ejecución dentro de un proyecto con `CppLibraryReference` sigue bloqueada por la guardia y no se presenta como verificada de extremo a extremo.

## Presupuesto de captura de demandas de funciones

- Archivos: `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`, `src/Frontend/ProjectNamespaceFunctionModel.cpp` y `tests/unit/CppClangSessionTests.cpp`.
- Añadir captura opcional al recorrido existente de llamadas y valores de función. Registrar proyecto, archivo, sintaxis, nombre escrito, namespace léxico, imports y alias visibles; distinguir llamada de valor. Sin captura, conservar el comportamiento y los resultados actuales. Estas demandas son candidatas sintácticas: un identificador también puede designar una variable o miembro y todavía no se afirma selección de sobrecarga.
- Probar llamadas directas, calificadas, mediante using y alias, contexto propietario, y ausencia de captura por defecto con sintaxis NativeSharp real. No conectar todavía al driver ni retirar guardias.
- Evidencia 2026-11-04: construcción ON, `nsc_cpp_session_tests.exe` y construcción `nsc_frontend OFF` terminaron con código 0. La prueba captura `c_add`, `tools.blend`, un `using`, un alias y `tools.blend` como valor; verifica que la captura sea opcional, se bloquee su cambio tras construir el modelo y conserve el contexto. Son referencias sintácticas candidatas, no decisiones de sobrecarga.

## Investigación de llamadas externas en análisis de cuerpos

- Alcance de reproducción: `tests/unit/CppClangSessionTests.cpp`; una función C escalar real extraída de Clang e inyectada al proyecto NativeSharp ordinario que llama `c_add(1, 2)`.
- Hipótesis a verificar: `ProjectMethodBodyModel` emite `MissingFunctionDefinition` porque `NamespaceFunctionSymbol::cppCanonicalIdentity` no se contempla como declaración externa. El resultado debe observarse antes de modificar análisis de cuerpos o HIR; no retirar las guardias de referencias de proyecto.
- Evidencia 2026-11-04: la prueba con `c_add` confirmó exactamente `MissingFunctionDefinition` y devolvió código 0 al verificar ese fallo esperado. `CallAnalyzer.cpp` acepta únicamente definiciones NativeSharp, `extern C` o funciones precompiladas, mientras que la proyección Clang conserva su identidad en `cppCanonicalIdentity` sin marcarse como esas categorías.
- Corrección autorizada por el objetivo: `src/Frontend/Bodies/CallAnalyzer.cpp` y `tests/unit/CppClangSessionTests.cpp`; reconocer la identidad Clang al verificar existencia de definición externa y comprobar análisis de cuerpos. Esto no altera la declaración LLVM ni demuestra ABI/enlace; las guardias permanecen.
- Evidencia 2026-11-04: construcción ON, `nsc_cpp_session_tests.exe`, construcción `nsc_frontend OFF` y `git diff --check` terminaron con código 0. El análisis de cuerpos acepta `c_add(1, 2)` y `tools.blend(1)` con declaraciones Clang inyectadas. Una declaración ordinaria NativeSharp sin definición sigue produciendo `MissingFunctionDefinition` al llamarla. HIR, convención de llamada y enlace siguen pendientes.

## Investigación de declaraciones importadas en HIR

- Reproducción autorizada en `tests/unit/CppClangSessionTests.cpp`: compilar hasta HIR un proyecto ordinario con funciones escalares C y C++ inyectadas desde Clang, y comprobar presencia del símbolo externo exacto y marca de importación.
- El código `ProjectOopHirModel::buildNamespaceFunction` sólo emite una declaración sin cuerpo si `isExternC` o `isPrecompiled`; la identidad Clang no está contemplada. Se comprobará esta hipótesis antes de cambiar HIR. La clasificación ABI completa y la emisión de convenciones distintas siguen pendientes; no se retirarán las guardias del proyecto.
- Evidencia previa 2026-11-04: la prueba dirigida devolvió código 1 porque tanto `c_add` como `?blend@tools@@YAHH@Z` faltaban en HIR, aunque el análisis de cuerpos había pasado.
- Cambio: `include/nativesharp/Frontend/NamespaceFunctionSymbol.h`, `src/Interop/Cpp/CppSemanticFunction.cpp`, `src/Frontend/ProjectNamespaceFunctionModel.cpp`, `src/Frontend/HIR/HirBuildCoordinator.cpp` y `tests/unit/CppClangSessionTests.cpp`. Preservar la convención reportada por Clang, validar su presencia y generar una declaración HIR externa con el símbolo exacto para la convención `cdecl` y firmas escalares ya proyectadas. Rechazar explícitamente otras convenciones en HIR hasta implementar su emisión; no equivale a soporte completo de ABI ni a una limitación definitiva del objetivo.
- Probar símbolos C y C++ y rechazo de `vectorcall` mediante sesiones reales. Mantener protegidos el enlace de proyectos mixtos, la caché y cualquier convención aún no implementada.
- Evidencia 2026-11-04: la construcción ON y `nsc_cpp_session_tests.exe` devolvieron código 0. Los símbolos `c_add` y `?blend@tools@@YAHH@Z` llegan como declaraciones a HIR; `vectorcall` se rechaza explícitamente. `git diff --check` devolvió código 0. La construcción OFF está en curso; falta comprobar MIR y no se ha demostrado ABI ni ejecución de llamadas mixtas.

## Comprobación de símbolos importados en MIR

- Prueba dirigida en `tests/unit/CppClangSessionTests.cpp`: continuar los dos casos C y C++ ya verificados hasta MIR y comprobar que conservan la marca de importación y el símbolo externo exacto. La implementación de `MirFunctionLowerer.cpp` copia esos campos desde HIR; la prueba verificará el flujo real. No modifica la política de ABI ni las guardias de compilación mixta.
- Evidencia 2026-11-04: el primer intento reutilizó una instancia de `ProjectCompilation` y falló porque es de un solo uso; se corrigió la prueba para crear instancias independientes. Construcción ON y ejecución de `nsc_cpp_session_tests.exe` devolvieron código 0; los dos símbolos y su marca se conservaron en MIR. La construcción `nsc_frontend OFF` también terminó con código 0. No se ha demostrado aún emisión LLVM, ABI, enlace ni ejecución mixta. Siguiente acción: investigar y verificar la emisión LLVM de estas declaraciones sin retirar las guardias de compilación mixta.

## Comprobación de declaraciones y llamadas LLVM

- Alcance de investigación: `tests/unit/CppClangSessionTests.cpp`. Continuar los casos escalares `c_add` y `tools.blend` desde MIR hasta un módulo LLVM, verificar el módulo y comprobar la declaración y llamada de cada símbolo externo exacto. `src/LLVM/Lowering/OopLlvmDeclarations.cpp` usa `mangledName` para la declaración y reconoce `isImportedC`; la emisión de la llamada aún debe probarse.
- No se cambian las guardias ni se afirma compatibilidad ABI general o ejecución. Si la prueba falla, registrar el resultado y localizar la causa antes de modificar código de producción.
- Evidencia 2026-11-04: construcción ON y ejecución de `nsc_cpp_session_tests.exe` devolvieron código 0. Para ambos casos, `ProjectOopLlvmLowerer` produjo un módulo verificado con la declaración y la llamada a `c_add` o `?blend@tools@@YAHH@Z`, respectivamente. Se usaron funciones escalares `cdecl` inyectadas; no se ha probado todavía generación de objeto, enlace ni ejecución de estas llamadas. Siguiente acción: investigar la generación del objeto y la forma reproducible de enlazarlo y ejecutarlo en A: sin retirar las guardias.

## Comprobación de objetos para llamadas escalares

- Archivo de prueba: `tests/unit/CppClangSessionTests.cpp`. Usar `LlvmTargetMachine::createWindowsX64` y `emit` para generar dos objetos en el directorio de fixtures de la prueba en A:, uno por cada módulo LLVM ya verificado. Revisar posteriormente sus símbolos con las herramientas de LLVM de A:. La emisión de un objeto no se presenta como enlace ni ejecución.
- Evidencia 2026-11-04: construcción y ejecución de `nsc_cpp_session_tests.exe` devolvieron código 0. `llvm-nm` identificó `c_add` y `?blend@tools@@YAHH@Z` como referencias externas en sus respectivos objetos, y ambos exponen `__nsc_function_Semantic$3A$3A__Main`. Falta enlace y ejecución.

## Experimento de enlace y ejecución de llamadas escalares

- Crear fuentes mínimas y un script en `nativesharp-interop/pruebas-llamadas-escalares/`, y sus artefactos bajo `A:/nsc-interop-build/cpp-fixtures/semantic/`. Cada ejecutable enlaza uno de los objetos generados por NSC con una función compilada por el Clang de A:. El arranque del experimento llama la función NativeSharp y usa `ExitProcess` para devolver éxito solo con el resultado esperado; se genera una biblioteca de importación de `kernel32.dll` con `llvm-dlltool` sin requerir SDK instalado. No se infiere soporte de arranque general, bibliotecas arbitrarias ni integración por proyecto.
- Evidencia 2026-11-04: `pruebas-llamadas-escalares/ejecutar.cmd` terminó con código 0. La función C calculó `100 * 1 + 2 = 102` y `tools::blend(int)` calculó `1 * 7 = 7`; cada ejecutable devuelve código 1 si el resultado difiere. La declaración se inyectó directamente a `ProjectCompilation` y el enlace se realizó de forma aislada con `lld-link`, por lo que el driver, las referencias declaradas en el proyecto, otras firmas ABI, DLL, inicialización general y el runtime integrado permanecen sin verificar. Siguiente acción: estudiar la conexión de demandas al proveedor del driver y conservar las guardias hasta verificar las demás fases.
- `git diff --check` en `A:/nsc-devel` terminó con código 0. El experimento utiliza el Clang y `lld-link` de A:, `llvm-dlltool` genera la biblioteca de importación en A:, y no instala archivos en C:.

## Cursor de investigación del proveedor de funciones

- El proveedor actual recibe grafo, sesiones y unidades sintácticas en `ProjectCompilationSnapshots.cpp`, antes de construir `ProjectNamespaceFunctionModel`. Las demandas se capturan durante la construcción de ese modelo, de modo que todavía no llegan al proveedor. `resolveFunctionCandidates` usa namespace léxico, imports y alias; reemplazarlo con búsqueda por nombre simple perdería precedencias y podría seleccionar funciones incorrectas. Siguiente acción: diseñar y probar la entrega de demandas al proveedor respetando esas reglas y snapshots antes de configurarlo en el driver. Las guardias permanecen activas.

## Entrega de demandas al proveedor semántico

- Archivos: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilationSnapshots.cpp`, `tests/unit/CppClangSessionTests.cpp`. Extender el callback con una vista de demandas; antes de invocarlo, construir un modelo observador con los mismos snapshots y opciones de captura que el modelo definitivo, con diagnósticos aislados. Si el observador falla, repetir la construcción con diagnósticos reales y terminar sin invocar al proveedor. Después de entregar demandas, construir el modelo final como antes. No se cambia la ruta sin proveedor, ni se conectan todavía la selección o el driver.
- Verificar llamadas y valores, proyecto y sintaxis, `using`, alias, fallo del proveedor y símbolos inválidos. Las demandas sólo existen durante la invocación y sus nodos pertenecen a las unidades retenidas por la compilación. Conservar guardias y compilación OFF.
- Evidencia 2026-11-04: construcción ON y ejecución de `nsc_cpp_session_tests.exe` devolvieron código 0; el proveedor recibió demandas para `c_add`, `using tools`, alias `T` y valor `tools.blend`. Una definición NativeSharp duplicada conserva su diagnóstico real y no invoca al proveedor. También pasaron las pruebas de fallo del proveedor y símbolos inválidos, la construcción `nsc_frontend OFF` y `git diff --check`. La copia de configuración de snapshots está implementada pero no se ha verificado con un proyecto precompilado en esta ruta. El driver todavía no selecciona funciones; las guardias permanecen. Siguiente acción: implementar y probar la selección de grupos a partir de demandas, respetando la búsqueda de namespaces existente y sin descartar silenciosamente sobrecargas no representadas.

## Candidatos de búsqueda asociados a cada demanda

- Archivos: `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`, `src/Frontend/ProjectNamespaceFunctionModel.cpp`, `tests/unit/CppClangSessionTests.cpp`. Capturar el nombre simple, los namespaces que podrían aportar funciones y los proyectos visibles; para nombres calificados reutilizar `resolveNamespace`, y para nombres sin calificar incluir namespace léxico e imports para que el modelo definitivo aplique su precedencia. Los candidatos no constituyen resolución de sobrecarga. Mantener las demás rutas y guardias sin cambio.
- Verificar demanda global, namespaces, alias y valores con las pruebas dirigidas; comprobar construcción OFF. La visibilidad entre proyectos debe verificarse por separado con un grafo de varias unidades antes de afirmar soporte completo.
- Evidencia 2026-11-04: `nsc_frontend OFF`, reconstrucción incremental `nsc_cpp_session_tests ON`, ejecución de ese binario y `git diff --check` devolvieron código 0. Se comprobó el nombre global `c_add`, `using tools`, alias `T` y el valor `tools.blend`. La primera construcción ON advirtió campos nuevos sin inicializador; se corrigió el inicializador y la reconstrucción del archivo terminó sin esa advertencia. Se comprobó presencia del proyecto propio en los candidatos; la visibilidad entre proyectos distintos sigue sin verificar. La selección del inventario y la conexión del driver permanecen pendientes.

## Selección de inventario por demanda

- Archivos: `include/nativesharp/Interop/CppSemanticFunctionGroups.h`, `src/Interop/Cpp/CppSemanticFunctionGroups.cpp` y `tests/unit/CppClangSessionTests.cpp`. Para cada demanda y cada combinación de namespace y proyecto visible, convertir el nombre de namespace al formato de Clang y reutilizar `selectCppSemanticFunctions`; devolver los grupos y los índices no proyectados junto con el índice de demanda, namespace y proyecto. Conservar conflictos y sus índices originales; no descartar sobrecargas no representadas ni decidir cuál gana.
- Verificar con inventarios Clang reales: overloads representables y no representados, proyecto visible y no visible, demandas repetidas y conflicto. Esta selección todavía no se conecta al driver ni implementa la resolución final de C++.
- Evidencia 2026-11-04: construcción ON y ejecución de `nsc_cpp_session_tests.exe` devolvieron código 0. El selector conservó dos sobrecargas proyectadas, una no proyectada y el índice de cada demanda, excluyó un proyecto fuera de la lista visible y propagó un conflicto con sus índices de inventario. La visibilidad fue suministrada por la prueba, no verificada aquí a través de un grafo real.
- Ampliación de prueba: reemplazar la selección fija `tools::blend` del proveedor de prueba por `selectCppSemanticDemands`, deduplicando símbolos y rechazando explícitamente los no representados dentro de ese experimento. El driver de producción sigue sin conectarse; esta prueba no define aún una política final de resolución para sobrecargas no proyectadas.
- Evidencia 2026-11-04: la construcción ON y la ejecución de `nsc_cpp_session_tests.exe` devolvieron código 0; las demandas generadas para `using`, alias y nombre calificado seleccionan las dos sobrecargas reales de Clang y las entregan al modelo sin fijar el nombre en el proveedor. Se verificó también `top.inner` frente a `top::inner` y se ejecutó `git diff --check` con código 0. La prueba usa un proveedor configurado explícitamente; no es aún el driver ordinario. El selector recorre el inventario por cada combinación de demanda, proyecto y namespace: falta medir y mejorar su costo con headers reales grandes. También falta resolver cómo reportar sobrecargas no proyectadas sin que una demanda sintáctica de una variable cause un falso error.

## Índice del inventario para selección por demanda

- Archivos: `src/Interop/Cpp/CppSemanticFunctionGroups.cpp` y `tests/unit/CppClangSessionTests.cpp`. Construir una vez por selección índices por proyecto/nombre y por proyecto/símbolo externo, preservando el orden e índices originales y la detección de alias incompatibles. Mantener `selectCppSemanticFunctions` como referencia de comportamiento; contrastar ambos para candidatos y conflictos. No modifica el driver ni la política de resolución.
- La complejidad esperada pasa de recorrer todo el inventario por cada búsqueda a indexarlo una vez y examinar las declaraciones y alias pertinentes. Medir el comportamiento con un inventario grande sigue pendiente.
- Investigación de diagnóstico diferido: `src/Frontend/Bodies/CallAnalyzer.cpp`, en torno a `referenceBinding(expression)`, comprueba si el nombre está oculto por una variable de scope o un miembro antes de tratarlo como función de namespace. `src/Frontend/Bodies/IdentifierAnalyzer.cpp` da prioridad a los miembros antes de tomar un nombre como grupo de funciones. Por eso un rechazo anticipado por toda demanda sintáctica puede producir errores falsos; el selector debe seguir conservando los candidatos no proyectados hasta una decisión semántica real. Aún no se ha implementado esa decisión.
- Evidencia 2026-11-04: la construcción ON y `nsc_cpp_session_tests.exe` terminaron con código 0. En el inventario Clang de la prueba se compararon las salidas del índice y la búsqueda anterior para todas las declaraciones, incluidas firmas no proyectadas; se verificó además el mismo conflicto entre dos nombres que comparten símbolo C. `git diff --check` terminó con código 0. No se han medido tiempos ni memoria con headers grandes. Siguiente acción: diseñar la conservación de candidatos no proyectados hasta análisis de cuerpos y comprobar sombreado por variables o miembros antes de conectar el driver.

## Investigación de punteros escalares C/C++

- `CppSemanticFunction.cpp` usa exclusivamente `mapCppScalarType`, que rechaza `CppTypeKind::Pointer`; NSC reconoce `builtin:int*` como puntero crudo en `TypeKeyPredicates.cpp`. Reproducir con una declaración Clang `int ptr_read(int*)` y exigir la representación `builtin:int*` antes de cambiar la conversión. Un puntero de 64 bits a escalares sin calificadores es el primer caso a comprobar; const/volatile, funciones, records, referencias y otras formas no se consideran demostradas. No se retirarán las guardias de integración.
- Para evitar perder cobertura de declaraciones no proyectadas, los fixtures de inventario que actualmente usan `int*` para ese propósito pasarán a usar punteros a records opacos, que continúan fuera de la proyección actual. Mantener las pruebas de conflicto y procedencia.
- Evidencia previa 2026-11-04: la construcción ON terminó con código 0 y la prueba devolvió código 1 exactamente en `C pointer to int maps to a NativeSharp pointer parameter`.
- Presupuesto de implementación: `src/Interop/Cpp/CppSemanticFunction.cpp` y `tests/unit/CppClangSessionTests.cpp`. Proyectar punteros de 64 bits a escalares o a `void`, sin calificadores en ningún nivel, conservando el tipo canónico C++ para identidad; validar primero semántica, HIR/MIR y LLVM. Las formas no representadas se siguen rechazando y deben conservarse en el inventario.
- Evidencia 2026-11-04: construcción ON y `nsc_cpp_session_tests.exe` devolvieron código 0. `int ptr_read(int*)` se proyecta como `builtin:int*`; una llamada NativeSharp con la dirección de una variable llega a MIR y a un módulo LLVM verificado, que se emite como objeto x64. Se preservaron los casos no proyectados usando records opacos. Falta verificar ejecución y el rechazo de punteros calificados.
- Ampliación de prueba: añadir al experimento de `pruebas-llamadas-escalares/` una implementación C de `ptr_read` y un ejecutable que compruebe el resultado; verificar además con declaraciones Clang que `const` y `volatile` no se eliminen silenciosamente y que `int**` conserve ambos niveles.
- Actualizar el comentario de contrato de `include/nativesharp/Interop/CppSemanticFunction.h` para describir el soporte nuevo sin afirmar ABI completa.
- Evidencia 2026-11-04: la construcción ON, `nsc_cpp_session_tests.exe`, `pruebas-llamadas-escalares/ejecutar.cmd` y `git diff --check` devolvieron código 0. El ejecutable NativeSharp pasó la dirección de una variable con valor 41 a `ptr_read`, compilada con Clang; la función leyó el puntero y devolvió 42. La prueba de proyección verificó `void*`, `int**` y el rechazo de `const int*`, `volatile int*` y `const int**`. Sólo se ejecutó el caso C `int*`; retorno de punteros, anidamiento y punteros C++ aún no se probaron en ejecución. La declaración se inyectó manualmente y el enlace fue aislado; driver, cualificadores y punteros a records siguen pendientes. Siguiente acción: validar el retorno de puntero y una función C++ con punteros, y continuar el diseño de candidatos no proyectados antes de retirar las guardias.

## Prueba de parámetro y retorno de puntero C++

- Archivos: `tests/unit/CppClangSessionTests.cpp` y `nativesharp-interop/pruebas-llamadas-escalares/`. Extraer con Clang la declaración `tools::pointer_identity(int*)`, proyectarla e inyectarla en un proyecto NativeSharp de prueba, comprobar MIR y LLVM, emitir un objeto en A:, enlazarlo con la implementación C++ real y ejecutar un programa que desreferencia el resultado. No retirar guardias ni presentar esta prueba como cobertura general de punteros o de bibliotecas C++.
- Evidencia 2026-11-04: la construcción ON, `nsc_cpp_session_tests.exe`, el script `pruebas-llamadas-escalares/ejecutar.cmd` y `git diff --check` terminaron con código 0. NativeSharp pasó la dirección de una variable con valor 41 a `tools::pointer_identity`, compilada con Clang; desreferenció el puntero devuelto y obtuvo 41. La prueba comprobó el símbolo C++ extraído de Clang en LLVM. La inyección y el enlace fueron manuales; siguen pendientes la integración por proyecto, punteros complejos y calificadores.
- Investigación para `const`: `MethodParameterSymbol` dispone de `pointerPointeeReadonly` y `CallAnalyzer.cpp` lo consulta al comprobar argumentos. `NamespaceFunctionSymbol` no tiene un campo equivalente para el retorno y `MethodBodyBuildCoordinator.cpp` copia `declaredTypeSyntax` y `declaredTypeKey` al adaptador. No se ha demostrado que un retorno `const T*` conserve su contrato; mantenerlo rechazado hasta completar y probar esa propagación. Siguiente acción: verificar el parámetro `const T*` por separado, con pruebas negativas de escritura y casos de sombreado, sin habilitar silenciosamente retornos calificados.

## Parámetros puntero a escalar const

- Reproducción: `tests/unit/CppClangSessionTests.cpp`, declaración Clang existente `int ptr_const(const int*)`. Comprobar que su parámetro se proyecta como `builtin:int*` y `pointerPointeeReadonly=true`; verificar una llamada con `int const*` y que la función mutable `ptr_read(int*)` no acepte ese puntero. Primero observar el fallo actual.
- Presupuesto: `src/Interop/Cpp/CppSemanticFunction.cpp`, su comentario en `include/nativesharp/Interop/CppSemanticFunction.h` y la prueba indicada. Soportar sólo `const` en el objeto escalar inmediatamente apuntado por un parámetro; seguir rechazando `volatile`, const anidado y retornos const hasta que sus contratos estén representados. No modificar comportamiento de NSC para proyectos ordinarios ni quitar las guardias.
- Evidencia previa 2026-11-04: compilación ON correcta y prueba con código 1 en `C const pointer parameter preserves pointee readonly`; Clang sí aportó la declaración, pero la proyección la rechazó.
- Ampliación de validación: comprobar el texto del diagnóstico de rechazo a `ptr_read` para descartar errores ajenos; llevar la llamada aceptada a MIR, LLVM y objeto, y enlazarla con una implementación `const int*` compilada por Clang en el experimento aislado de A:.
- Evidencia 2026-11-04: construcción ON, ejecución de `nsc_cpp_session_tests.exe` y `pruebas-llamadas-escalares/ejecutar.cmd` devolvieron código 0. Un `int const*` llamó a `ptr_const(const int*)` y el ejecutable devolvió 42; al usar el mismo argumento con `ptr_read(int*)`, NSC produjo el diagnóstico específico de ausencia de sobrecarga viable. `const int**`, `volatile int*` y el retorno `const int*` siguen rechazados explícitamente. Las referencias se inyectaron manualmente y el enlace fue aislado.
- Investigar la comparación entre sobrecargas `f(int*)` y `f(const int*)`: ambos usan `builtin:int*` como clave NS, pero el primer parámetro es mutable y el segundo readonly. Reproducir con Clang y comprobar si un argumento mutable selecciona correctamente la primera sobrecarga antes de afirmar compatibilidad con ese caso.
- Evidencia 2026-11-04: la construcción y `nsc_cpp_session_tests.exe` devolvieron código 0 al comprobar el fallo esperado: con `tools::qualify(int*)` y `tools::qualify(const int*)`, la llamada NativeSharp con `&value` se informa como ambigua. Esto no es interoperabilidad correcta y no cierra el caso. Añadir una referencia compilada por Clang que compruebe mediante `static_assert` la selección de ambas variantes, antes de diseñar la corrección.
- Evidencia de referencia 2026-11-04: `pruebas-llamadas-escalares/ejecutar.cmd` compiló con el Clang de A: `cpp-const-overload-reference.cpp`, cuyos `static_assert` requieren seleccionar `int*` para un argumento mutable y `const int*` para uno readonly; el script y `git diff --check` terminaron con código 0. Siguiente acción: investigar la clasificación y comparación de candidatos importados para delegar o reproducir comprobablemente esta regla sin cambiar la resolución de sobrecargas NativeSharp ordinarias.

## Desempate de sobrecargas C++ de puntero mutable y const

- Reproducción: `tools::qualify(int*)` y `tools::qualify(const int*)` con argumento `int*`; NSC informó ambigüedad, mientras Clang confirmó la elección mutable. Causa: `betterCandidate` compara claves de tipo y rango, pero ambos parámetros tienen `builtin:int*` y el contrato readonly queda en otro campo.
- Presupuesto: `src/Frontend/Bodies/CallAnalyzer.cpp` y `tests/unit/CppClangSessionTests.cpp`. Aplicar desempate local exclusivamente entre dos candidatos importados C++ de un parámetro, con conversión exacta del mismo puntero no nulo y sin argumentos por defecto ni variádicos; preferir el parámetro mutable si el otro agrega const. La viabilidad previa excluye el mutable cuando el argumento es readonly. No alterar `betterCandidate` compartido, llamadas NativeSharp ni otros casos de C++.
- Verificar elección efectiva para argumento mutable y readonly contra Clang, y conservar ambigüedad para `null`; ejecutar regresiones dirigidas. Este tratamiento parcial no constituye resolución completa de sobrecargas C++.
- La prueba dirigida ya selecciona las variantes esperadas para argumentos mutable y readonly, y conserva la ambigüedad para `null`. Ampliar el experimento para emitir un programa que llame ambas variantes y enlace con funciones C++ distintas; agregar una comprobación de ambigüedad de `nullptr` a la referencia Clang.
- Evidencia 2026-11-04: construcción ON, `nsc_cpp_session_tests.exe`, construcción `nsc_frontend OFF`, `pruebas-llamadas-escalares/ejecutar.cmd` y `git diff --check` devolvieron código 0. La prueba inspeccionó la función elegida para cada argumento y verificó ambigüedad para `null`; un ejecutable enlazado a dos implementaciones C++ devolvió 12, demostrando que se llamaron las variantes mutable y const respectivamente. La referencia Clang también verificó que `nullptr` es ambiguo. El resultado sólo cubre la conversión exacta de un argumento y usa inyección y enlace aislados; no demuestra resolución C++ general ni la ruta ordinaria del driver. Siguiente acción: integrar la información de sobrecargas no proyectadas sin falsos errores y avanzar hacia la compilación real de referencias de proyecto.

## Conservación autónoma de candidatos no proyectados

- Archivos: `include/nativesharp/Interop/CppSemanticFunctionGroups.h`, `src/Interop/Cpp/CppSemanticFunctionGroups.cpp` y `tests/unit/CppClangSessionTests.cpp`. Convertir los índices no proyectados de la selección en registros propios que conserven sintaxis de demanda, proyecto consumidor y productor, referencia, declaración copiada y motivo de falta de proyección. Validar índices, procedencia y variante antes de copiar; no emitir diagnósticos ni tomar una decisión de sobrecarga en esta etapa.
- Verificar con una demanda sintáctica real y sesión Clang real, incluyendo una sobrecarga no representada, que los datos sobrevivan a la destrucción del inventario temporal y que entradas inconsistentes se rechacen. La sintaxis permanece propiedad de la compilación. Esto prepara la comunicación con el frontend; aún no conecta el driver ni resuelve automáticamente la llamada.
- Evidencia 2026-11-04: construcción ON, `nsc_cpp_session_tests.exe` y `git diff --check` terminaron con código 0. Una demanda real de `tools.handle` conservó la declaración Clang de puntero a record opaco, su motivo, proyecto, índice de referencia y sintaxis después de destruir el inventario temporal. También se rechazaron índice de inventario inválido, índice de demanda inválido, sintaxis nula, proyecto fuera de la visibilidad e intento de etiquetar como no proyectada una entrada proyectada. No se ha trasladado todavía esta información al modelo de cuerpos ni se ha probado la selección con tipos no representados. Siguiente acción: conectar el resultado autónomo al frontend y verificar que variables o miembros que sombrean un nombre no produzcan un rechazo anticipado.

## Transferencia de candidatos no proyectados al frontend

- Archivos: nuevo `include/nativesharp/Interop/CppDeferredFunctionCandidate.h`; `include/nativesharp/Interop/CppSemanticFunctionGroups.h`, `include/nativesharp/Frontend/ProjectCompilation.h`, `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`, `src/Frontend/ProjectCompilationSnapshots.cpp`, `src/Frontend/ProjectNamespaceFunctionModel.cpp` y `tests/unit/CppClangSessionTests.cpp`. Mover el registro autónomo a un encabezado sin dependencia de Clang, permitir que el proveedor entregue registros al frontend y conservarlos en el modelo de namespaces. Validar pertenencia a la demanda, proyecto visible y datos esenciales antes de aceptarlos.
- Pruebas: sesión y sintaxis reales con candidato no proyectado, vida más allá del inventario, rechazo de registros inválidos y una variable local que comparte el nombre sin generar error anticipado. No producir diagnósticos ni seleccionar una sobrecarga por la mera presencia del registro. El driver ordinario y el análisis de cuerpos siguen pendientes; las guardias permanecen.
- Evidencia 2026-11-04: construcciones ON y `nsc_frontend OFF`, ejecución de `nsc_cpp_session_tests.exe` y `git diff --check` terminaron con código 0. Una sesión Clang real entregó una función C no proyectada llamada `shadow`; el modelo del frontend conservó su declaración y motivo mientras una variable local del mismo nombre se analizó sin error. Se rechazaron sintaxis nula, referencia fuera de rango y nombre que no corresponde a la demanda. La prueba usa un proveedor explícito y el modelo sólo conserva los registros: aún no diagnostica llamadas reales ni decide sobrecargas. No se ha probado sombreado por miembros o múltiples proyectos. Siguiente acción: conectar el proveedor real del driver y diseñar el consumo diferido sin aceptar silenciosamente sobrecargas no proyectadas; mantener las guardias hasta verificar la ruta completa.

## Proveedor de funciones para el driver

- Archivos: `include/nativesharp/Interop/CppSemanticFunctionGroups.h`, `src/Interop/Cpp/CppSemanticFunctionGroups.cpp`, `src/Driver/PrecompiledModuleConfiguration.cpp` y `tests/unit/CppClangSessionTests.cpp`. Extraer una operación común y transaccional que recopile sesiones, seleccione demandas, conserve candidatos no proyectados y deduplique símbolos proyectados por proyecto, namespace y símbolo. Configurarla en el driver junto al proveedor temprano de namespaces cuando existan sesiones.
- Probar esa misma operación con sesiones y sintaxis reales tanto en llamadas C++ representadas como en sombreado por variable local. La ruta ordinaria sigue bloqueada por las guardias; no se declara funcional. El selector todavía puede reportar conflictos durante una demanda sólo sintáctica: es una limitación conocida que debe resolverse antes de habilitar la ruta ordinaria.
- Evidencia 2026-11-04: construcciones `nsc_cpp_session_tests ON` y `nsc_driver OFF`, ejecución de `nsc_cpp_session_tests.exe` y `git diff --check` terminaron con código 0. La configuración del driver con sesiones reales instaló el proveedor; la operación compartida resolvió llamadas C++ por `using`, alias y nombre calificado, conservó el candidato no proyectado que comparte nombre con una variable y mantuvo intactas las salidas ante un error de inventario. No se ejecutó el proveedor a través de una compilación ordinaria con `CppLibraryReference`, que sigue detenida por las guardias; las declaraciones no proyectadas tampoco participan todavía en la resolución de cuerpos. Siguiente acción: diferir conflictos y decisiones sobre candidatos no proyectados hasta reconocer el uso semántico real, después verificar enlace y ruta completa antes de retirar guardias.

## Protección temporal contra selección incompleta de sobrecargas

- Reproducción en `tests/unit/CppClangSessionTests.cpp`: sesión Clang real con `tools::mix(int)` y `tools::mix(Opaque*)`, una llamada `tools.mix(1)` y el proveedor real. Antes de implementar, comprobar si NSC elige la primera sin considerar la segunda. La resolución completa deberá delegarse a Clang; un diagnóstico temporal no satisface el objetivo final.
- Presupuesto de corrección: `src/Frontend/Bodies/CallAnalyzer.cpp` y prueba dirigida. Sólo cuando una llamada se haya reconocido como función de namespace y no esté sombreada, impedir que se seleccione silenciosamente un conjunto que contiene candidatos no proyectados para la misma función visible. No diagnosticar una variable o miembro que oculte el nombre ni modificar compilaciones sin candidatos diferidos. Mantener las guardias de proyectos mixtos.
- Evidencia previa 2026-11-04: construcción ON correcta y ejecución con código 1 en `real call cannot silently ignore an unprojected C++ overload`; la llamada aceptaba la sobrecarga `int` aunque el inventario conservaba también `Opaque*` sin proyectar.
- Evidencia posterior 2026-11-04: construcción ON, `nsc_frontend OFF`, `nsc_cpp_session_tests.exe` y `git diff --check` terminaron con código 0. La llamada `tools.mix(1)` produce el diagnóstico temporal cuando el conjunto de sobrecargas incluye `Opaque*` sin proyectar. Una variable local y un método NativeSharp que sombrean sobrecargas importadas no reciben ese diagnóstico; una función global NativeSharp tiene precedencia frente a una declaración no proyectada bajo `using tools`. Sólo se protege el caso en que existe al menos una función representada y una llamada reconocida como namespace; llamadas con todos los candidatos no proyectados, valores de función y conflictos tempranos siguen pendientes. El diagnóstico es una medida provisional, no resolución C++ ni interoperabilidad completa. Las guardias siguen activas.

## Diferir conflictos activados sólo por sintaxis

- Reproducción en `tests/unit/CppClangSessionTests.cpp`: dos sesiones Clang con declaraciones C incompatibles del mismo símbolo y un programa NativeSharp donde una variable local usa ese nombre. Comprobar primero que el proveedor rechaza anticipadamente el programa, aunque no se llame a la función.
- Presupuesto propuesto: `include/nativesharp/Interop/CppSemanticFunctionGroups.h`, `include/nativesharp/Interop/CppDeferredFunctionCandidate.h`, `src/Interop/Cpp/CppSemanticFunctionGroups.cpp` y la prueba indicada. Mantener la interfaz estricta existente y añadir selección diferida para el proveedor: conservar ambas declaraciones y su procedencia asociadas a la demanda, sin proyectar un símbolo ambiguo ni diagnosticar por la sola referencia sintáctica. La comprobación de uso real y las guardias siguen pendientes.
- Evidencia previa 2026-11-04: construcción ON con código 0 y ejecución con código 1 en `local shadowing does not eagerly reject conflicting C declarations`.
- Para que la procedencia de ambas declaraciones pueda verificarse en la frontera del frontend, incluir también `src/Frontend/ProjectCompilationSnapshots.cpp` en la modificación y validar el proyecto y referencia de cada origen del conflicto.
- Evidencia 2026-11-04: construcciones ON y `nsc_frontend OFF`, ejecución de `nsc_cpp_session_tests.exe` y `git diff --check` terminaron con código 0. El programa con una variable local `shadow` ya compila aunque dos sesiones Clang declaren el símbolo C de forma incompatible; el frontend conserva ambas firmas y sus referencias 0 y 1, y rechaza una referencia de origen alterada. La selección estricta sigue reportando el conflicto; la variante diferida también conservó los índices de un conflicto entre alias C. No se ha probado aún el diagnóstico de una llamada real cuando todos los candidatos están diferidos, ni la materialización de alias en el frontend. Las guardias siguen activas; diferir el conflicto no equivale a resolverlo.

## Llamada a declaraciones C incompatibles diferidas

- Alcance: reproducir una llamada real `shadow(42)` con dos declaraciones C incompatibles y sin candidato proyectado; comprobar que se informe el conflicto al usarlo sin anticiparlo para variables o métodos NativeSharp que ocultan el nombre.
- Presupuesto: `tests/unit/CppClangSessionTests.cpp` y, si la reproducción lo demuestra, `src/Frontend/Bodies/CallAnalyzer.cpp`, exclusivamente para llamadas no calificadas sin binding. Se preservan los comportamientos anteriores y las guardias de compilación. Las llamadas calificadas y el soporte efectivo de las declaraciones diferidas permanecen pendientes.
- Evidencia 2026-11-04: la construcción ON pasó y la prueba de llamada real falló con `actual call reports conflicting deferred C declarations` antes de la corrección. `analyzeNamespaceCallHead` comprueba ahora el conflicto diferido de esa llamada sin binding después de respetar nombres locales y métodos del tipo contenedor. La prueba completa de sesiones pasó después, incluyendo una llamada a un método NativeSharp con el mismo nombre y las dos declaraciones C incompatibles; la construcción de `nsc_frontend` OFF y `git diff --check` también pasaron. El diagnóstico identifica un conflicto, no constituye soporte de esas declaraciones. No se verificaron aquí receptores calificados ni valores invocables locales.
- Siguiente acción: reproducir y resolver con precedencia correcta una llamada donde todos los candidatos están diferidos por un tipo aún no proyectado; conservar el análisis de nombres locales y receptores calificados. La integración real por el driver y el retiro seguro de las guardias continúan pendientes.

## Proyección de funciones C variádicas

- Objetivo: aprovechar la representación y promociones variádicas ya existentes en NSC para proyectar una declaración C `cdecl` obtenida de Clang, preservando su identidad y comprobando primero la propagación semántica; la ejecución real debe verificarse antes de afirmar que la capacidad está soportada.
- Presupuesto inicial: `src/Interop/Cpp/CppSemanticFunction.cpp` y `tests/unit/CppClangSessionTests.cpp`. Las funciones C++ variádicas y demás convenciones permanecen fuera de esta modificación hasta demostrar su semántica; el resto de las protecciones se conserva.
- Verificación de ejecución: extender `pruebas-llamadas-escalares/ejecutar.cmd` y agregar un fixture C++ de implementación externa en la misma carpeta. Emitir desde el ensayo un objeto NativeSharp que llame la función con argumentos enteros y flotantes promovidos, enlazarlo con el objeto generado por Clang y ejecutar el resultado en A:.
- Evidencia 2026-11-04: la proyección antes fallaba y ahora conserva el indicador variádico. La reproducción hasta MIR falla en `acceptNamespaceCallArgumentChild`: rechaza un ordinal posterior a los parámetros fijos antes de aplicar la conversión ya seleccionada por `classifyCVariadicPromotion`. Para corregir esta ruta se amplía el presupuesto a `src/Frontend/HIR/ExpressionStatementMachine/HirCallFrameLowering.cpp`, exclusivamente para argumentos variádicos de funciones importadas desde Clang; las llamadas ordinarias conservan su ruta actual. Se verificará el LLVM y la ejecución antes de considerar disponible esta capacidad.
- Evidencia posterior 2026-11-04: se proyectó `bridge_probe(size_t, ...)` desde una sesión Clang C real; se conservan identidad, convención y carácter variádico. El frontend aplica las promociones seleccionadas antes de generar HIR; el módulo LLVM verificado declara `i32 @bridge_probe(i64, ...)` y emite un objeto Windows x64. Un ejecutable de prueba enlazó ese objeto con una implementación Clang que comprueba la promoción `byte` a `int` y `float` a `double` mediante `va_arg`; la ejecución y el resto de pruebas de llamadas pasaron con código 0. El primer enlace sin CRT reveló la referencia `_fltused`; el fixture de prueba la define explícitamente. Construcción ON, frontend OFF y `git diff --check` pasaron. Una declaración C++ variádica sigue diferida y se verifica ese límite. La prueba inyecta el símbolo directamente: la ruta ordinaria con `CppLibraryReference` y sus guardias aún no está habilitada; no se ha verificado una cobertura general de tipos o ABI variádico.
- Siguiente acción: avanzar hacia una compilación ordinaria de proyecto mixto verificando entrada, selección, enlace y caché con un fixture real; mantener visibles las limitaciones de las declaraciones aún no proyectadas.

## Bibliotecas C/C++ en la lista de enlace

- La inspección 2026-11-04 encontró que `ProjectLoader::loadCppLibraryReferences` conserva y valida la biblioteca, pero `validateProjectLibraryReferences` únicamente añade `LibraryReference` con metadatos NS. Por ello la biblioteca de `CppLibraryReference` no llega a la lista de entrada del enlazador.
- Presupuesto: `src/Driver/LibraryReferenceValidation.cpp` y `tests/unit/CppClangSessionTests.cpp`. Agregar bibliotecas C/C++ no vacías en orden de grafo, deduplicándolas con la misma identidad de ruta ya usada por el driver; una referencia de solo header no agrega una ruta. Conservar el rechazo de compilación y caché; la copia y validación de DLL es trabajo separado y no se considera resuelta.
- Reproducción: cargar el proyecto real del fixture con una referencia que tiene biblioteca y otra de solo header, invocar la preparación de bibliotecas con el grafo ya cargado y verificar el resultado y la deduplicación. No declarar operativo el flujo ordinario con base en esta prueba aislada.
- Evidencia 2026-11-04: la prueba falló antes del cambio con `link inputs include each C/C++ archive once and omit header-only references`. Después, `appendValidatedLibraryReferences` incluyó una vez la biblioteca obtenida del proyecto, omitió la referencia de solo header y mantuvo una sola entrada al ejecutarse dos veces. La prueba completa de sesiones ON, la construcción de `nsc_driver` OFF y `git diff --check` pasaron. Esto verifica la lista de entradas, no el enlazado de un proyecto completo ni el tratamiento de DLL; las guardias de compilación y caché siguen vigentes.
- Siguiente acción: conectar y verificar la ruta real con sesiones completas y referencias del proyecto, diseñando previamente cómo evitar reutilización de caché sin identidad de headers transitivos; después comprobar el enlace real y la preparación de DLL.

## Análisis de un proyecto con sesiones Clang

- El driver admite `--no-artifact-cache`; conservar el bloqueo de identidad para proyectos mixtos cuando la caché está activa. No se considera suficiente desactivar únicamente el bundle, porque también existen cachés de objetos y frontend.
- Objetivo de esta verificación: analizar hasta MIR un proyecto que declara `CppLibraryReference`, con sesiones abiertas desde sus referencias y los proveedores reales. Rechazar proyectos sin proveedores completos, con propietario ausente, cantidad incorrecta o encabezado distinto; conservar las pruebas actuales para proyectos no configurados. El enlace y ejecución ordinarios, las DLL y la caché permanecen pendientes.
- Presupuesto: `src/Frontend/ProjectCompilation.cpp`, `tests/unit/CppClangSessionTests.cpp` y nuevos archivos de fixture bajo `A:/llvm-project-al82dev/nativesharp-interop/pruebas-proyecto/`. No se modifica el bloqueo de caché.
- Evidencia 2026-11-04: antes del cambio, el proyecto con sesión y proveedores reales falló con `C/C++ project references are not yet connected`. Después, `ProjectCompilation` permitió analizarlo hasta MIR, donde se comprobó la declaración importada `project_sum`. Comprueba que cada proyecto con referencias tenga ambos proveedores, la cantidad de sesiones, encabezado, lenguaje, estándar, triple Windows x64, macros e includes de la referencia. Una sesión de buffer diferente y un proyecto sin proveedores se rechazaron; el caso anterior sin configuración sigue rechazándose. Construcciones ON y frontend OFF, prueba de sesiones y `git diff --check` pasaron. Las sesiones se configuraron desde el ensayo, no desde una invocación ordinaria; no se ha probado enlace ni ejecución de este proyecto. La identidad de caché sigue bloqueando los proyectos mixtos con caché habilitada.
- Siguiente acción: construir el ejecutable NSC ON en A:, generar una biblioteca de prueba y ejecutar la ruta ordinaria con `--no-artifact-cache`, revisando las opciones de recursos Clang y la propagación real al enlazador; identificar y corregir los bloqueos que produzca esa ruta antes de afirmar que está operativa.

## Proyecto mixto por consola

- La prueba usará exclusivamente `A:/nsc-interop-build` para binarios y `A:/llvm-project-al82dev/nativesharp-interop/pruebas-proyecto/` para sus fuentes y script. Compilar `api.c` con Clang 23.1.0 y archivarlo con LLVM; copiar el proyecto al directorio de prueba, compilarlo con `nsc.exe` ON y `--no-artifact-cache`, enlazar y ejecutar. No desplegar en C: ni activar caché.
- Los cambios al compilador se decidirán a partir del fallo real de ese recorrido y se registrarán antes de aplicarlos. La prueba utiliza una función C escalar y no certifica todavía el resto de la interoperabilidad ni DLL.
- Evidencia 2026-11-04: el proyecto compiló y devolvió 3 usando la biblioteca creada por Clang. El primer intento requirió configurar `NSC_WINDOWS_RUNTIME_LIBRARY` al runtime generado en A:. La verificación adicional con `--artifact-cache` compiló indebidamente: `releaseProjectGraph()` mueve el grafo antes de que `calculateIdentity()` recorra `graph_`, así que esa protección no ve las referencias del grafo cerrado. Este resultado invalida la afirmación previa de que la caché estaba protegida para la invocación ordinaria.
- Corrección autorizada por la reproducción: en `src/Driver/PrecompiledModuleConfiguration.cpp`, comprobar el grafo original en `configurePrecompiledModules` antes de transferirlo cuando la caché de artefactos esté habilitada; mantener el rechazo hasta identificar headers transitivos y demás dependencias. Extender `pruebas-proyecto/ejecutar.cmd` para comprobar que el caso con caché explícita falla por esa razón y que el caso sin caché sigue ejecutándose.
- Evidencia posterior 2026-11-04: `pruebas-proyecto/ejecutar.cmd` compiló `api.c` con Clang de A:, creó `api.lib`, invocó el NSC ON con el proyecto y `--no-artifact-cache`, enlazó y ejecutó con resultado 3. El ensayo usa el runtime generado en A: y una copia en A: del `nslink.exe` de `C:/native-sharp/bin`; no modifica la instalación original. Con `--artifact-cache` explícita, la misma invocación falló con `C/C++ project references require --no-artifact-cache until transitive headers are tracked`; el script verifica ese mensaje. Sin configuración Clang el proyecto también se rechaza. Construcción de NSC ON, driver OFF, prueba de sesiones ON y `git diff --check` pasaron. El enlace de esta función C escalar es evidencia de extremo a extremo únicamente para esta muestra; no cubre C++ complejo, DLL ni caché de proyectos mixtos.
- Siguiente acción: agregar un proyecto ordinario que consuma una función C++ con nombre decorado y ejecutar el enlace; después avanzar hacia clases y ciclo de vida y hacia una identidad de caché que incorpore dependencias transitivas.

## Proyecto C++ ordinario con sobrecargas

- Reproducir en `pruebas-proyecto/` un proyecto separado que importe un encabezado C++ con namespace y sobrecargas, compile su biblioteca con Clang de A:, use `tools.blend(3)` desde NativeSharp y ejecute el binario generado por el NSC ON. Guardar todos los artefactos en `A:/nsc-interop-build` y conservar la política de caché existente.
- La prueba debe verificar la presencia del símbolo C++ decorado en la biblioteca, no insertar manualmente declaraciones en NSC y comprobar el resultado de ejecución. Si falla, registrar la causa y acotar los cambios antes de realizarlos. El resultado solo verifica las declaraciones y tipos presentes en este caso.
- Evidencia 2026-11-04: `pruebas-proyecto/ejecutar-cpp.cmd` compiló una biblioteca con dos sobrecargas de `tools::blend` usando Clang 23.1.0 y comprobó sus símbolos `?blend@tools@@YAHH@Z` y `?blend@tools@@YANN@Z`. NSC leyó la referencia desde el proyecto, enlazó la biblioteca y ejecutó ambas llamadas sin bindings escritos a mano: `tools.blend(3) + (int)tools.blend(1.5)` devolvió 36. El ensayo usa `--no-artifact-cache` y una copia de NSLink en A:. Las otras sobrecargas, clases, DLL y cobertura completa de C++ no se consideran verificadas.
- Siguiente acción: investigar la proyección de tipos y métodos C++ de instancia, en particular representación, construcción y destrucción, y preparar una prueba real de esa ruta sin introducir wrappers manuales como interfaz del usuario.

## Firma y calificador de métodos C++

- Inspección: `CppMethodDeclaration` conserva el símbolo y `const`, pero `collectRecords` no expone el tipo de función canónico, la convención ni la calificación de referencia `&`/`&&`; esa información es necesaria para decidir si una invocación es válida y emitirla con el ABI correcto.
- Presupuesto: `include/nativesharp/Interop/CppRecordDeclaration.h`, `src/Interop/Cpp/ClangSession.cpp` y `tests/unit/CppClangSessionTests.cpp`. Conservar desde Clang el tipo canónico, la convención y la calificación de referencia sin suponer que ya son invocables. Probar sobrecargas de métodos `&` y `&&` y un método estático `__vectorcall`, además de los métodos existentes.
- Evidencia 2026-11-04: antes de completar la extracción, la prueba de sesión falló al buscar los tres datos. Después, una sesión Clang real conservó tipos canónicos y convenciones `cdecl`/`vectorcall` y distinguió las sobrecargas `&` y `&&`; la prueba completa ON, el frontend OFF y `git diff --check` pasaron. El primer intento también reveló que el nuevo archivo `CppMain.ns` interfería con el glob del proyecto anterior; se renombró a `CppMain.ns.txt` y el script lo copia como `Main.ns` solo en el directorio de prueba. Ambas pruebas de proyecto volvieron a pasar. La metadata capturada todavía no importa ni invoca métodos de instancia.
- Siguiente acción: definir y probar la identidad y representación de records importados dentro de NSC, incluyendo layout y vida de objeto, antes de conectar la selección y emisión de constructores y métodos.

## Tipos C++ anidados y su acceso

- Inspección 2026-11-04: `collectRecords` recorre namespaces y especificaciones de enlace, pero no entra en records; por ello un tipo anidado público o privado desaparece del inventario de Clang. `CppRecordDeclaration` tampoco conserva su accesibilidad.
- Presupuesto: `include/nativesharp/Interop/CppRecordDeclaration.h`, `src/Interop/Cpp/ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp`. Capturar records anidados nombrados no dependientes, conservar acceso y nombre calificado, y probar anidamiento público y privado, definición posterior a declaración adelantada y layout. No declarar que esto importe tipos al frontend ni que resuelva su ciclo de vida.
- Evidencia 2026-11-04: antes del cambio, la sesión real falló en `nested records retain qualified identity, access, definition and layout`. Después identificó `Outer::Inner` público con definición adelantada y tamaño 32 bits una sola vez, e `Outer::Secret` privado; la prueba de sesiones ON, frontend OFF y `git diff --check` terminaron con código 0. El recorrido conserva el acceso que Clang reporta en el record y entra en definiciones completas; aún no se ha conectado la declaración al sistema de tipos NativeSharp ni verificado la accesibilidad transitiva de tipos anidados.
- Siguiente acción: diseñar y conectar las identidades de records al resolvedor de tipos de NSC con sus dueños y visibilidad; verificar primero tipos de valor y punteros antes de emitir llamadas de constructores o destructores.

## Identidad estructurada de records C++

- El nombre calificado con `::` no distingue por sí solo namespaces y records contenedores, mientras que `TypeIdentity` distingue `.` y `+`; reconstruir esa distinción a partir de una cadena produciría colisiones. Conservar los dos tipos de contexto directamente desde `DeclContext`, así como la presencia de namespaces o records anónimos.
- Presupuesto: `include/nativesharp/Interop/CppRecordDeclaration.h`, `src/Interop/Cpp/ClangSession.cpp`, nuevos `include/nativesharp/Interop/CppSemanticRecord.h` y `src/Interop/Cpp/CppSemanticRecord.cpp`, `CMakeLists.txt`, `tests/unit/CppClangSessionTests.cpp`. Proyectar una identidad NSC únicamente cuando todos sus componentes son representables; probar `A::B::C` con `B` namespace y con `B` record en sesiones distintas, con identidades distintas, y rechazar la proyección de contextos anónimos. Esto todavía no registra tipos en el resolvedor ni define layout o vida de objeto.
- Evidencia 2026-11-04: dos sesiones Clang reales con `A::B::C` produjeron las claves distintas `Semantic::A.B+C` y `Semantic::A.B.C`; un tipo en namespace anónimo y un módulo vacío no se proyectaron. El adaptador conserva segmentos de namespace y records contenedores por separado y usa `compilation::TypeIdentity` para validar la identidad. Construcción y pruebas ON, frontend OFF y `git diff --check` pasaron. La identidad aún no se incorpora al resolvedor, no cubre templates y no implica interoperabilidad de objetos.
- Siguiente acción: incorporar las identidades de records al índice de nombres de proyecto preservando propietario, accesibilidad, visibilidad entre proyectos y conflictos; verificar resolución desde una declaración NativeSharp antes de habilitar creación o paso por valor.

## Índice experimental de nombres de records importados

- Objetivo: permitir configurar explícitamente identidades de records en una instancia de `ProjectNameLookup` y comprobar que una anotación de tipo NativeSharp las resuelve con el índice normal. Las identidades deben pertenecer a un proyecto existente y su módulo, conservar clase/struct y acceso, participar en la detección ordinaria de duplicados y mantenerse dentro del índice del proyecto propietario.
- Presupuesto: `include/nativesharp/Frontend/ProjectNameLookup.h`, `src/Frontend/ProjectNameLookup.cpp`, `tests/unit/CppClangSessionTests.cpp`. La configuración es explícita y de prueba; no conectarla al driver ni presentar estos tipos como seguros para uso por valor, construcción o destrucción mientras no exista su layout y ciclo de vida.
- Evidencia 2026-11-04: una sesión Clang produjo la identidad `Semantic::A.B.C`; una instancia configurada explícitamente de `ProjectNameLookup` resolvió `A.B.C*` en un parámetro NativeSharp al ordinal de ese record. Se rechaza la instalación posterior al build, un proyecto propietario inexistente y una colisión con una declaración NativeSharp. La primera prueba mostró que el agregador exige conservar una ruta de origen; se agregó la ruta del encabezado obtenida de Clang. Una variable local no se incluyó en esta comprobación porque la fase de nombres ensayada no produjo un binding para esa anotación; la prueba usa una firma de función. Pruebas ON, construcción de frontend OFF, `git diff --check` y proyectos C/C++ por consola con el NSC actualizado pasaron.
- Límites: el índice experimental aún requiere inyección explícita y no está conectado al driver; no se han comprobado referencias entre proyectos, accesibilidad anidada, parámetros de plantillas, layout ni ciclo de vida. La resolución de nombres no demuestra que un objeto pueda usarse de forma segura.
- Siguiente acción: construir el proveedor de records para el grafo con sesiones reales, deduplicación y visibilidad, y conectar su salida al frontend antes de resolver firmas, manteniendo un rechazo explícito de operaciones de objeto hasta implementar su representación.

## Inventario de records del grafo

- Objetivo inmediato: recopilar todos los records de las sesiones de cada proyecto con su ruta, índice de referencia, ordinal propietario y triple. Conservar la declaración y su identidad proyectada opcional; una identidad anónima o no representable no se elimina del inventario. No deduplicar todavía: dos encabezados pueden dar definiciones incompatibles y la selección debe comprobarlas antes de exponerlas.
- Presupuesto: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp`. Probar sesiones reales de varios proyectos, dos referencias, y fallos de propietario o sesión cerrada. No conectar aún el resultado al driver ni interpretar este inventario como soporte de clases.
- Evidencia 2026-11-04: la prueba de sesiones ON pasó con dos proyectos, un módulo precompilado insertado antes de ellos y varias sesiones reales. El inventario conservó dos definiciones incompatibles de `Shared` de 32 y 64 bits con sus índices de referencia, un record del segundo proyecto y otro en namespace anónimo sin identidad proyectada; también identificó la sesión cerrada y rechazó un propietario ausente. La primera construcción falló porque faltaba incluir la definición de `ProjectGraph`; se agregó esa dependencia y la reconstrucción pasó. `git diff --check` pasó. El inventario aún no reconcilia definiciones ni alimenta el resolvedor o driver.
- Siguiente acción: diseñar y probar la reconciliación segura de declaraciones del mismo record entre sesiones, conservando discrepancias y considerando que la descripción actual no representa todas las bases ni otras propiedades C++; después conectar la selección al índice sin habilitar aún operaciones cuyo layout y vida no están implementados.

## Bases directas de records C++

- `CppRecordDeclaration` no conserva bases, por lo que la misma lista de campos no basta para comparar o representar dos clases. Registrar las bases directas en orden, su tipo canónico, acceso efectivo, carácter virtual y desplazamiento de subobjeto informado por el layout de Clang para la clase completa.
- Presupuesto: `include/nativesharp/Interop/CppRecordDeclaration.h`, `src/Interop/Cpp/ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp`. Verificar herencia ordinaria y virtual, acceso, orden y offsets con sesiones Clang. No interpretar los offsets como ABI completo ni habilitar todavía herencia, conversiones o construcción de objetos en NSC.
- Evidencia 2026-11-04: antes de la extracción falló `direct C++ bases preserve order, access, virtual status and Clang offsets`. Después, una sesión Clang conservó dos bases directas en orden, `public`/`private`, con offsets 0 y 32 bits, y una base directa virtual con su offset disponible; la suite ON, frontend OFF y `git diff --check` pasaron. Se almacenan únicamente bases directas; aún faltan el grafo completo, ajustes de `this`, despacho virtual, construcción/destrucción y verificación del comportamiento en ejecución.
- Siguiente acción: usar este inventario al reconciliar declaraciones de records, conservando conflictos y procedencia; no suponer equivalencia completa solo porque coincidan las bases y tamaños observados.

## Huella ODR de definiciones de records

- Clang 23.1.0 dispone de `clang::ODRHash` para detectar diferencias entre definiciones de records; para C++ incluye subdeclaraciones y bases. Capturar esa huella solo para definiciones que admite dicha API y conservarla en el inventario. Una huella distinta identifica una discrepancia; una huella igual de 32 bits no demuestra equivalencia por posibles colisiones ni sustituye validación de ABI o contexto.
- Presupuesto: `include/nativesharp/Interop/CppRecordDeclaration.h`, `src/Interop/Cpp/ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp`. Probar sesiones con definiciones idénticas y con campos distintos del mismo tamaño, además de conservar la ausencia de huella para declaraciones incompletas. No habilitar aún deduplicación automática ni clases por valor.
- La revisión de `ODRHash::AddCXXRecordDecl` muestra que Clang omite especializaciones de plantillas y sus contextos. Como la identidad actual tampoco conserva argumentos de especialización, se amplía el presupuesto a `src/Interop/Cpp/CppSemanticRecord.cpp`: marcar esos contextos y mantener su declaración en el inventario sin proyectar una identidad que colisione con el tipo primario, hasta implementar plantillas.
- Evidencia 2026-11-04: la prueba falló antes de capturar la huella y pasó después. Dos sesiones con `struct Same { int x; }` produjeron huellas iguales; una con `float x` produjo una huella distinta pese a tener el mismo tamaño. Un record C completo tuvo huella y los incompletos no; una especialización `Box<int>` quedó inventariada sin huella ni identidad proyectada. Construcciones ON y frontend OFF, suite de sesiones y `git diff --check` pasaron. Las coincidencias de huella no se usan para declarar equivalencia ni deduplicar.
- Siguiente acción: agrupar candidatos por propietario e identidad, detectar diferencias comprobadas de huella y layout conservando sus orígenes, y tratar como no resuelta la equivalencia de definiciones con huellas iguales hasta disponer de comparación suficiente.

## Agrupación y conflictos de records

- Agrupar por ordinal propietario e identidad NSC, conservando todos los índices del inventario. Conservar aparte entradas sin identidad. Detectar como conflicto diferencias de triple y, entre definiciones completas, diferencias de tamaño, alineación o huella ODR disponibles; no inferir equivalencia de huellas iguales.
- Clasificar grupos sin definición, con una sola definición, con varias definiciones todavía no verificadas y con conflicto; no elegir silenciosamente un ganador ni habilitar importación automática. Presupuesto: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp`.
- Evidencia 2026-11-04: las pruebas ON agruparon sesiones reales de dos proyectos; conservaron todos los índices de declaraciones, incluyendo las que no tienen identidad proyectable. Detectaron diferencias de layout y de huella con tamaños iguales, una discrepancia de triple, declaraciones adelantadas con y sin definición, y clasificaron definiciones con huellas iguales como equivalencia no verificada. `git diff --check` pasó. La agrupación es infraestructura y todavía no selecciona definiciones ni alimenta el índice de nombres del driver.
- Siguiente acción: diseñar la selección por demanda y el tratamiento de definiciones repetidas con evidencia de procedencia y contexto; conectar al índice solo cuando se mantenga un modelo seguro para tipos externos, evitando tratar una clase C++ como struct NativeSharp completo.

## Observación de demandas de tipos en NSC

- Comprobar con un proyecto y sesión Clang reales si una primera ejecución aislada de `ProjectNameLookup` conserva el nombre no resuelto, su sintaxis y proyecto sin contaminar los diagnósticos de la compilación; después contrastar con la resolución del mismo nombre una vez registrado el record.
- Presupuesto de esta reproducción: `tests/unit/CppClangSessionTests.cpp`. No añadir selección heurística por nombres ni conectar el driver hasta conocer cómo se conservan namespaces, alias y precedencia de tipos NativeSharp.
- Evidencia 2026-11-04: una instancia aislada de `ProjectNameLookup` reportó el nombre no resuelto `A.B.C` junto con su sintaxis y ordinal de proyecto, mientras que los diagnósticos de la compilación original permanecieron limpios. Al registrar el record desde la sesión Clang, la misma firma resolvió al ordinal del tipo importado. La prueba completa ON y `git diff --check` pasaron. La observación no verificó todavía `using`, alias, tipos anidados no públicos ni tipos locales dentro de cuerpos.
- Siguiente acción: comprobar el comportamiento de los `using` y alias con el resolvedor real antes de decidir el mecanismo de selección; evitar comparar directamente cadenas de nombres de C++ con la sintaxis NativeSharp.
- Evidencia adicional 2026-11-04: las firmas con `using A.B` y `using X = A.B` conservaron la demanda no resuelta en una pasada con diagnósticos separados y resolvieron al record correcto cuando se registró. La lectura de `SyntaxBinding.cpp` y una prueba con `using Alias = A.B.C` y destino registrado confirmaron que el resolvedor expande alias modificando el `TypeSyntax` compartido. La primera prueba sin registrar el destino no produjo esa expansión; no se generaliza ese resultado. Por tanto, separar diagnósticos **no** hace de `ProjectNameLookup::build` una observación sin efectos: no se incorporará esa pasada sobre el AST de producción sin aislar o rediseñar esa mutación. La suite ON y `git diff --check` pasaron.
- Siguiente acción revisada: diseñar una observación que preserve el AST de producción y la semántica de alias, incluyendo fuentes, tipos conocidos y propietarios; o integrar el registro de candidatos en una sola pasada. Comparar ambas opciones con pruebas antes de conectar el driver.

## Unidades aisladas para observar tipos

- El parser usa `collectParserKnownTypeNames` para compartir los tipos y alias de todas las unidades; reparsear cada archivo sin ese conjunto alteraría la interpretación. Crear copias de observación desde los buffers ya capturados, conservando ordinales, módulo, `FileId` y conjunto compartido de tipos conocidos. Las copias deben tener arenas independientes y sus diagnósticos deben quedar separados; ante un buffer ausente o error, no devolver una salida parcial.
- Presupuesto: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp` y `tests/unit/CppClangSessionTests.cpp`. La prueba observará un alias sobre la copia, verificará que el AST original no se altere y contrastará la resolución posterior. No conectar todavía esta pasada al driver ni afirmar paridad de snapshots precompilados sin comprobarla.
- Evidencia 2026-11-04: `cloneSyntaxForObservation` reparseó el buffer ya procesado de la compilación usando el mismo conjunto de nombres conocidos y arenas nuevas. La prueba expandió `Alias = A.B.C` al resolver la copia mientras el `TypeSyntax` original conservó `Alias`; los diagnósticos originales permanecieron limpios. Un proyecto sin parsear falló sin reemplazar una salida previa. Las construcciones ON y frontend OFF, la suite de sesiones y `git diff --check` pasaron. La función rechaza expresamente unidades precompiladas: no se ha comprobado su reconstrucción ni la paridad en múltiples archivos o con todos los productos de sintaxis.
- Una comprobación posterior ejecutó la resolución normal sobre el AST original después de observar la copia: el alias se expandió y vinculó al record esperado; la suite ON volvió a pasar.
- Siguiente acción: verificar dos o más unidades con tipos conocidos cruzados y el tratamiento de unidades precompiladas; después usar las copias para seleccionar records realmente referenciados con el resolvedor sin mutar el AST de producción.

## Verificación de observación con varias unidades

- Objetivo: contrastar las unidades originales y sus copias para un proyecto de dos archivos con un tipo NativeSharp definido en un archivo y usado mediante alias en otro. Comparar identidad y resolución sin mutar el árbol original durante la observación.
- Presupuesto: `tests/unit/CppClangSessionTests.cpp`, archivos temporales del proyecto de prueba bajo `A:/nsc-interop-build/cpp-fixtures/` y este registro. No modificar el frontend ni habilitar unidades precompiladas sin evidencia de equivalencia.
- Investigación de unidades precompiladas: `ProjectCompilation::materializePrecompiledUnit` puede restaurar un producto sintáctico publicado o en caché; la equivalencia de reparsearlo desde el buffer no está demostrada. Se conserva el rechazo existente.
- Evidencia 2026-11-04: la prueba genera en A: un proyecto de dos archivos, con `game.Node` definido en uno y el alias `N` usado en el otro. Las dos copias conservaron ordinales, módulo y `FileId`, con árboles independientes. El lookup de la copia resolvió el tipo y dejó el alias original intacto; el lookup definitivo lo expandió y resolvió después. La construcción ON, la suite `nsc_cpp_session_tests` y `git diff --check` terminaron con código 0. Este caso no demuestra paridad general ni compatibilidad con módulos precompilados.
- Siguiente acción: estudiar cómo asociar las vinculaciones del lookup aislado con los grupos de records de Clang, conservando visibilidad y conflictos, antes de conectarlas al frontend de producción.
- Investigación adicional: `TypeNameBinding` conserva el ordinal del tipo resuelto y el proyecto; `ExpressionTypeNameBinding` conserva candidatos por separado, sin ordinal de proyecto propio. `MemberAccessAnalyzer.cpp` y `ExpressionCore/PrimaryExpressions.cpp` deciden durante el análisis de cuerpos si esos candidatos corresponden al receptor y si son ambiguos. Por tanto, tomar todos los candidatos de expresiones como usos confirmados podría diagnosticar tipos C++ aunque una variable u otra interpretación tenga prioridad. `NamedTypeDeclaration` tampoco identifica por sí mismo su origen C++; la asociación debe conservar el grupo y propietario al registrar candidatos. No se ha conectado selección al driver.
- Siguiente acción concreta: reproducir con el lookup aislado un receptor estático C++ y un identificador sombreado por una variable NativeSharp; comprobar dónde se descarta el candidato antes de definir la selección, y mantener los diagnósticos diferidos hasta disponer de evidencia.

## Receptores de expresión y sombreado

- Presupuesto: `tests/unit/CppClangSessionTests.cpp` y este registro; reutilizar el proyecto semántico ya generado. Comparar el lookup aislado de un record C++ real con y sin una variable local del mismo nombre y contrastar el análisis de cuerpos de un caso NativeSharp equivalente. No cambiar aún la selección ni el compilador.
- Evidencia 2026-11-04: con `using A.B`, el lookup aislado encontró el record real `A.B.C` como candidato para `C.value` tanto sin variable local como con una variable `C` de tipo `Holder`. En un caso NativeSharp equivalente, `compileProjectMethodBodies` registró `TypeReference` para `C.Get()` sin variable y `Local` cuando `C` es una variable local, sin registrar `TypeReference` en ese caso. `ProjectMethodBodyModel::resolveMemberReceiver` comprueba `lookupScopeName` y `memberValueShadowsType` antes de `resolveWrittenMemberType`. Construcción ON, suite de sesiones y `git diff --check` terminaron con código 0. El caso de cuerpos usa tipos NS y no prueba todavía métodos de records C++.
- Consecuencia de diseño: las referencias resueltas en firmas pueden asociarse al grupo de records por ordinal e identidad; los candidatos de receptores necesitan permanecer diferidos hasta conocer la resolución del cuerpo. `ProjectNameLookup::build` incorpora los tipos C++ como `pendingTypes_` y `NamedTypeDeclaration` no conserva una marca explícita de origen: antes de tratarlos como externos hay que mantener una asociación verificable entre la declaración instalada y el grupo, sin inferirla exclusivamente por la cadena de nombre.
- Siguiente acción: diseñar una asociación explícita entre grupo C++ e identidad instalada en el lookup aislado, probar referencias de tipo, conflictos y candidatos diferidos; todavía no habilitar objetos C++ ni conectar al driver.

## Procedencia de tipos C++ en el índice de nombres

- Objetivo: permitir que un consumidor del lookup consulte qué entrada de `setCppImportedTypes` produjo un ordinal de tipo, sin atribuir esa procedencia a tipos NativeSharp ni publicar una asociación tras una construcción fallida.
- Presupuesto: `include/nativesharp/Frontend/ProjectNameLookup.h`, `src/Frontend/ProjectNameLookup.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. El índice guardará únicamente ordinal e índice de la entrada instalada; no elegirá grupos conflictivos, no interpretará candidatos de expresiones como usos confirmados y no habilitará objetos ni integrará el driver.
- Comprobaciones: tipo C++ registrado con una resolución real; tipo NativeSharp sin procedencia C++; intento con propietario inválido y conflicto con una declaración NS sin asociación publicada; builds ON y OFF.
- Evidencia 2026-11-04: `ProjectNameLookup::cppImportedTypeIndex` permite consultar el índice de la entrada registrada a partir de un ordinal. La construcción forma un mapa temporal después de resolver, comprueba propietario, parte sintáctica y ruta, y solo lo publica si no hubo errores. La suite verificó un record de una sesión Clang real, un tipo NativeSharp sin procedencia externa, un propietario inexistente y una colisión con declaración NativeSharp. Construcción `nsc_cpp_session_tests ON`, construcción `nsc_frontend OFF`, suite de sesiones y `git diff --check`: código 0.
- Límite: la procedencia identifica una entrada instalada, todavía no un grupo completo de declaraciones Clang; no activa tipos C++ en el driver ni resuelve selección de expresiones o módulos precompilados.
- Siguiente acción: generar las entradas y su correspondencia explícita con grupos de records, conservar estados de conflicto e incluir solo los grupos vinculados por referencias de tipos; mantener los candidatos de expresiones como diferidos.

## Asociación de referencias con grupos de records

- Implementar un selector que reciba el lookup ya construido y una correspondencia explícita de cada entrada instalada a su grupo. Validar ordinal, propietario e identidad antes de utilizarla; conservar el índice del grupo y con él su estado de conflicto y procedencia. Las referencias de tipo resueltas se recopilan por separado de los candidatos de expresiones, que siguen diferidos.
- Presupuesto: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. No interpretar la selección como soporte de layout o vida, no conectar el driver y no rechazar candidatos de expresiones como si fueran usos confirmados.
- Evidencia 2026-11-04: `selectCppRecordBindings` cruza la procedencia explícita del lookup con los índices de grupos proporcionados; comprueba entradas faltantes, índices inválidos o duplicados, identidad y proyecto, y devuelve listas deduplicadas de referencias resueltas y candidatos de expresión. La prueba seleccionó un grupo en estado `Conflict` sin perder ese estado, rechazó asociaciones inválidas y conservó como diferidos los candidatos `C.value`, tanto si `C` está sombreado como si no. Construcción ON, suite de sesiones con Clang y `git diff --check` terminaron con código 0.
- Límite: la correspondencia de entradas a grupos aún la proporciona el llamador; no está implementada su generación automática ni conectada al driver. La prueba de conflicto verifica que la selección conserve el estado, no que ya exista un diagnóstico al usar la clase. No se ha demostrado soporte de objetos C++.
- Siguiente acción: producir entradas observables y correspondencias a partir del inventario y los grupos reales, preservando acceso, categorías y orígenes sin confundir el placeholder de observación con la representación ABI definitiva; probar múltiples referencias y conflictos antes de conectar el driver.

## Preparación de entradas para observación de records

- Crear entradas para `ProjectNameLookup` a partir de grupos e inventario, acompañadas por sus índices originales. Validar que cada miembro pertenece al grupo y conservar fuera de esa lista los grupos cuya accesibilidad o ubicación no se puede representar con seguridad; no declarar que un `struct` de observación sea un layout NativeSharp válido. Los conflictos siguen en sus grupos y no se descartan.
- Presupuesto: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro; no conectar al driver ni habilitar clases u operaciones por valor.
- Evidencia 2026-11-04: `prepareCppRecordObservation` verifica cada índice y su identidad y proyecto, produce en paralelo entradas de lookup y sus índices de grupo, conserva conflictos y deja explícitamente diferidos grupos anidados o sin ubicación. Los inventarios de sesiones Clang de dos proyectos preservaron las rutas y propietarios; se comprobó un grupo con tres definiciones conflictivas y se rechazó un índice fuera del inventario. Una segunda prueba conectó un record real de Clang con la preparación automática, un lookup aislado y `selectCppRecordBindings`, conservando un conflicto de triple. Los grupos anidados y sin ubicación se probaron como diferidos. Construcción ON, suite de sesiones y `git diff --check`: código 0.
- Alcance: las entradas usan `Struct` como representación temporal de nombres a nivel de namespace, no como descripción de la categoría ni del ABI C++; el inventario original conserva esos datos. No se conectó al driver, ni se habilitó una clase, ni se resolvieron visibilidad o categorías de records anidados.
- Siguiente acción: integrar esta observación aislada al flujo de compilación manteniendo prohibidas las operaciones de objetos hasta implementar su representación y diagnostics de conflictos por demanda; verificar proyectos con sesiones reales, colisiones con tipos NS y módulos precompilados antes de habilitarla en el driver.

## Observación de records sobre una compilación real

- Encadenar inventario, grupos, preparación, namespaces, copia sintáctica, lookup y selección en una operación explícita sobre `ProjectCompilation` ya parseada. El resultado conserva inventario, grupos y selección; un fallo de la observación devuelve su causa sin contaminar los diagnósticos o el AST de producción.
- Presupuesto: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Comprobar sesión Clang ligada a proyecto, colisión con un tipo NS y proyecto sin parsear. No instalar en el driver hasta resolver colisiones de tipos no usados y la compatibilidad con unidades precompiladas.
- Evidencia 2026-11-04: `observeCppRecords` recopila sesiones asociadas al proyecto, agrupa y prepara records, obtiene namespaces, clona la sintaxis y ejecuta lookup y selección con diagnósticos independientes. Una prueba con una sesión real resolvió un tipo C++ usado mediante alias y verificó que el alias original no cambiara. Otra demostró que una colisión entre tipos NS y C++ devuelve un fallo de observación sin añadir errores a la compilación; un proyecto no parseado también devuelve fallo. Construcción ON, suite de sesiones y `git diff --check`: código 0.
- Límite: la colisión también provoca fallo si el tipo C++ no se usa; el observador completo no se conectó al driver. `cloneSyntaxForObservation` rechaza unidades precompiladas y todavía no se ha verificado su paridad. No hay soporte nuevo de objetos C++ por esta operación.
- Siguiente acción: resolver la colisión de candidatos no utilizados sin perder los usos reales ni ocultar ambigüedades; incorporar pruebas de acceso con y sin demanda antes de conectar el observador al flujo del driver.

## Colisiones durante observación aislada

- Causa: `observeCppRecords` instala todos los candidatos antes de agregar los tipos NS; `ProjectNameLookup::finalizePredeclarations` diagnostica una identidad duplicada incluso si el programa no la referencia. Hacer una observación previa de tipos NS en copias independientes y retirar solo esas entradas C++ del segundo índice, conservando sus grupos en el resultado y registrando por separado referencias de tipo y candidatos de expresión que apunten a la identidad en conflicto.
- Presupuesto: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. No elegir entre las dos declaraciones como resolución definitiva, no conectar el driver ni modificar el índice de producción. Verificar colisión sin uso, uso en firma y caso C++ no colisionado; mantener las unidades precompiladas rechazadas.
- Evidencia 2026-11-04: `observeCppRecords` construye primero un índice NS sobre una copia, identifica colisiones por identidad y proyecto y reconstruye otra copia para la observación final sin insertar los candidatos en conflicto. Conserva por separado todos los grupos colisionados, los referenciados por firmas y los candidatos en expresiones; el último conjunto no prueba que una expresión resuelva finalmente al tipo. Una tabla por ordinal evita recorrer todas las vinculaciones por cada colisión. Se verificaron colisión no utilizada, tipo colisionado en firma, candidato colisionado en expresión y record C++ sin colisión; también el alias original y los diagnósticos de producción permanecieron intactos. Construcción ON, suite de sesiones y `git diff --check`: código 0.
- Límite: el resultado conserva la colisión pero no decide todavía qué lenguaje gana ni emite un diagnóstico definitivo; el driver no está conectado. La preobservación puede contener errores de tipos no resueltos, por lo que solo se utiliza su índice de declaraciones NS; la segunda observación sigue exigiendo que su lookup termine sin errores. Los módulos precompilados permanecen sin soporte en esta observación.
- Siguiente acción: comprobar las colisiones entre proyectos visibles y los candidatos ocultos por variables en esta operación completa; diseñar el consumo del resultado en el flujo real sin asumir que un candidato de expresión es una referencia confirmada.

## Visibilidad y sombreado en la observación completa

- Verificar un proyecto consumidor con referencia a un proyecto productor y una sesión Clang asociada al productor. Comparar un tipo C++ visible, una declaración NS homónima del consumidor y una colisión NS/C++ dentro del productor. Comprobar también que un candidato de expresión con variable local homónima permanezca diferido.
- Presupuesto: `tests/unit/CppClangSessionTests.cpp`, fixtures generados bajo `A:/nsc-interop-build/cpp-fixtures/` y este registro. No conectar al driver ni modificar el comportamiento de producción en esta prueba.
- Evidencia 2026-11-04: la suite generó tres pares de proyectos con `ProjectReference` y una sesión Clang propiedad del productor. Se seleccionó el tipo C++ cuando era visible y no había tipo NS homónimo; un tipo NS del consumidor tuvo prioridad sin marcar una colisión de identidad entre módulos; cuando el tipo NS y el C++ pertenecieron al productor, el grupo quedó marcado como colisionado y referenciado desde el consumidor. Otro caso con una variable local homónima mantuvo la colisión entre candidatos de expresión sin convertirla en referencia de tipo confirmada. Construcción ON, suite de sesiones y `git diff --check`: código 0.
- Alcance: los casos prueban la observación y prioridades del lookup, no la ejecución de miembros C++. Los candidatos de expresión siguen sin confirmar hasta análisis de cuerpos, y la observación aún falla ante otros errores de resolución o módulos precompilados.
- Siguiente acción: diseñar el contrato que entregue la observación al frontend sin publicar placeholders como tipos ABI y sin bloquear proyectos C++ existentes por limitaciones de la observación; conservar el resultado completo para la resolución posterior por demanda.

## Observación de unidades precompiladas

- Objetivo: copiar las unidades precompiladas a una arena independiente usando el codec sintáctico existente, conservando su identidad y la marca precompilada; configurar los snapshots semánticos al construir ambos índices de observación. Mantener el reparsado actual para las unidades de fuente ordinarias.
- Presupuesto: `src/Frontend/ProjectCompilation.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Verificar un módulo precompilado real dentro del grafo, un record C++ de una sesión y la independencia de los árboles. No habilitar aún placeholders como objetos ni conectar el driver.
- Evidencia 2026-11-04: la reproducción con un `PrecompiledSyntaxModule` sintético insertado en el grafo falló inicialmente con `syntax observation requires valid non-precompiled source units`. `cloneSyntaxForObservation` ahora serializa y restaura la unidad precompilada en una arena independiente y conserva su marca y `FileId`; las unidades ordinarias siguen reparsándose. Los índices de observación reciben los snapshots semánticos disponibles en la compilación. La prueba resolvió en la misma firma `Published.Value*` del módulo sintético y `A.B.C*` de una sesión Clang. Construcción ON, frontend OFF, suite de sesiones y `git diff --check`: código 0.
- Límite: el fixture construye el módulo mediante la API del frontend y no contiene un producto publicado `.nsproducts` ni un snapshot semántico cargado; pasar estos datos al lookup está implementado pero su paridad con snapshots publicados todavía no está verificada. Tampoco se ha integrado la observación al driver.
- Siguiente acción: probar un módulo con snapshot publicado real y comprobar la equivalencia del lookup aislado con el de producción; después conectar un proveedor de observación al flujo del frontend sin instalar los placeholders como representación de objeto.

## Validación con snapshot publicado

- Generar una biblioteca de prueba y su `.nsmeta`/`.nsproducts` mediante NSC en `A:/nsc-interop-build/cpp-fixtures/`, preparar un consumidor con `LibraryReference` y exigir que el snapshot se cargue de verdad antes de comparar el lookup de producción con la observación aislada.
- Presupuesto: `tests/unit/CppClangSessionTests.cpp`, `A:/llvm-project-al82dev/nativesharp-interop/pruebas-snapshot/`, artefactos en `A:/nsc-interop-build/` y este registro. No escribir en C:, no alterar el driver ni publicar productos fuera de los fixtures de A:.
- Hallazgo previo a ejecutar: `PrecompiledSnapshotUnit::index` contiene punteros al AST original y `PrecompiledSnapshotReader` los utiliza al instalar nombres. La conexión anterior de los snapshots originales al lookup sobre copias no garantiza aislamiento. Se incluye `src/Interop/Cpp/CppSemanticRecord.cpp` en el presupuesto para reconstruir el índice del snapshot contra cada copia y verificar identidad de `FileId`, cantidad y digest antes de utilizarlo.
- Evidencia 2026-11-04: `observeCppRecords` reconstruye ahora el índice de cada snapshot sobre cada copia, verifica `FileId`, cantidad de nodos, cantidad de tipos y digest antes de instalarlo. `pruebas-snapshot/ejecutar.cmd` construye `nsc` y la prueba en la misma invocación, emite una biblioteca real con `.nsmeta` y `.nsproducts`, y comprueba que el consumidor cargó un snapshot, instaló nombres desde él, resolvió `Published.Value` y completó la observación de una sesión Clang. El script completo y `git diff --check` terminaron con código 0.
- La primera ejecución detectó una diferencia entre identificadores de compilación porque `nsc` y la prueba se habían enlazado en construcciones separadas; por ello el script los construye juntos. El primer intento de emisión también requirió apuntar `NSC_WINDOWS_RUNTIME_LIBRARY` al runtime generado en A:. Ambos problemas se corrigieron en el procedimiento de prueba.
- Límite: se verificó un producto publicado con snapshot de nombres y un tipo de biblioteca; no se afirma equivalencia de todos los modelos ni está integrado el observador al driver. Siguiente acción: conectar un proveedor opcional al frontend que conserve la observación sin publicar sus placeholders como objetos, y validar que las compilaciones sin referencias C++ se mantienen idénticas.

## Punto de observación en el frontend

- Agregar a `ProjectCompilation` un observador opcional que se configura antes de compilar y recibe la compilación como solo lectura después de generar las unidades y antes de resolver los nombres de producción. El llamador conserva el resultado propio; el observador no publica tipos ni cambia la validez de la compilación. Sin observador no se ejecuta trabajo adicional.
- Presupuesto: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Probar invocación única, sesión Clang real, snapshot publicado, configuración tardía rechazada y ruta sin observador. Mantener la integración del driver y la representación de objetos para pasos posteriores.
- Evidencia 2026-11-04: `setCppRecordObserver` acepta el callback solo antes de iniciar la compilación; `compileProjectNames` lo invoca después de generics y antes del lookup de producción. Una prueba conservó el resultado de una sesión Clang, verificó una sola invocación, ausencia de placeholders en el lookup final y rechazo de configuración tardía. La prueba de snapshot publicado también ejecutó la observación mediante ese punto, con snapshot instalado. Construcción ON y frontend OFF, `pruebas-snapshot/ejecutar.cmd`, y los proyectos C y C++ existentes (resultados 3 y 36) terminaron con código 0; `git diff --check` pasó.
- Límite: el driver todavía no configura el observador y el resultado no habilita llamadas ni objetos C++ nuevos. El callback es una interfaz interna de solo lectura sobre la compilación; su consumidor debe conservar y evaluar sus resultados sin convertir candidatos de expresión en usos confirmados. Siguiente acción: diseñar la integración del resultado con selección de tipos y tratamiento por demanda en el driver, manteniendo intactas las compilaciones que no usan records.

## Identidad de records en firmas C++

- Para conectar firmas como `Node*` con el grupo de records correcto, conservar en `CppType` la identidad estructurada del `RecordDecl` canónico proporcionado por Clang: nombre, namespaces, records contenedores y marcas de anonimato/especialización. Las cadenas de tipo por sí solas no distinguen con seguridad los contextos.
- Presupuesto: `include/nativesharp/Interop/CppType.h`, `src/Interop/Cpp/ClangSession.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Probar declaraciones incompletas, punteros a tipos anidados, alias, namespace anónimo y especialización, sin habilitar aún su ABI ni modificar el driver.
- La siguiente conexión requiere comparar la identidad estructurada del tipo en la firma con la identidad del inventario, sin interpretar texto. Se agregan `include/nativesharp/Interop/CppSemanticRecord.h` y `src/Interop/Cpp/CppSemanticRecord.cpp` para convertir esa identidad mediante las mismas restricciones que la declaración; los contextos anónimos y especializados siguen sin proyectarse.
- Evidencia 2026-11-04: `CppType::recordIdentity` conserva la identidad del `RecordDecl` canónico que Clang encuentra incluso en tipos incompletos y a través de aliases. `projectCppRecordTypeIdentity` la transforma en identidad NSC sin analizar la cadena de tipo. La prueba comparó directamente la identidad de los parámetros `Node*` y `Outer::Inner*` con sus declaraciones inventariadas; verificó alias equivalentes y mantuvo anónimos y especializaciones identificados como contextos no proyectables. Construcción ON, suite de sesiones y `git diff --check`: código 0.
- Límite: esta información aún no conecta `Node*` con el análisis de llamadas ni habilita objetos C++. Siguiente acción: relacionar cada record referenciado por una firma con el grupo propietario y sus conflictos, para preparar parámetros y retornos sin confundir identidades entre proyectos.

## Dependencias de records en firmas de funciones

- Extraer de cada retorno y parámetro sus records bajo punteros, referencias y arreglos. Asociarlos por propietario y `TypeIdentity` al grupo inventariado; conservar explícitamente registros no proyectables o ausentes y el índice de la función, parámetro y profundidad. Un grupo conflictivo permanece identificado, sin suponer que se puede llamar con él.
- Presupuesto: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Probar sesiones reales de dos proyectos con nombres iguales, conflicto, declaración incompleta, puntero múltiple y especialización. No cambiar la selección del driver ni habilitar el ABI todavía.
- Evidencia 2026-11-04: `collectCppSignatureRecordDependencies` indexa grupos por propietario e identidad y recorre retorno y parámetros a través de punteros, referencias y arreglos. La prueba con sesiones Clang reales asoció `Shared*` en retorno y parámetro a un grupo conflictivo del primer proyecto, `Shared**` con profundidad 2, un tipo adelantado al grupo `ForwardOnly`, y otro `Shared*` al segundo proyecto. Una especialización quedó como `Unprojectable`; quitar un grupo produjo `Missing`; un grupo duplicado y un árbol de tipo incompleto se rechazaron. Construcción ON, suite de sesiones y `git diff --check`: código 0.
- Límite: las dependencias no se usan todavía para proyectar las funciones ni confirmar la seguridad ABI; tampoco se han modelado argumentos de templates ni tipos de función anidados. Siguiente acción: usar esta asociación para proyectar punteros a records sin perder procedencia, manteniendo una barrera que impida tratarlos como objetos por valor hasta implementar su representación.

## Proyección aislada de firmas con punteros a records

- Añadir una ruta explícita de proyección de funciones que reciba los grupos de records, respete su propietario y acepte únicamente identidades sin conflictos demostrados. Se conserva la ruta existente para el driver: esta preparación aislada no habilita aún llamadas ni operaciones sobre objetos C++.
- Archivos previstos: `include/nativesharp/Interop/CppSemanticFunction.h`, `src/Interop/Cpp/CppSemanticFunction.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Comprobar punteros, calificación constante, propiedad, conflictos y rechazo de valores por valor; ejecutar la suite de sesiones y verificar el diff.
- Evidencia 2026-11-04: `projectCppSemanticFunctionWithRecords` prepara firmas con punteros a records de identidad y propietario conocidos; admite declaraciones adelantadas o una definición, conserva `const` del parámetro y rechaza valores, especializaciones no proyectables, conflictos, definiciones aún no verificadas, grupos duplicados o ausentes. La suite usa sesiones Clang reales con dos proyectos y comprueba esos casos. La primera construcción detectó un acceso equivocado al nombre del módulo en la prueba; se corrigió usando el grafo de proyectos. Reconstrucción ON, suite de sesiones y `git diff --check`: código 0.
- Límite: la proyección sigue aislada y no prueba que NSC pueda representar records opacos ni emitir llamadas a funciones que los reciban. Siguiente acción: identificar y diseñar las restricciones en resolución y generación de código que permitan pasar esos punteros sin habilitar acceso por valor, construcción o destrucción antes de implementar esas capacidades.

## Corrección de identidad nominal en firmas

- Hallazgo: `ProjectMemberModel::canonicalNamedTypeKey` antepone `type:` a la identidad nominal; la proyección aislada comparaba su resultado con una clave formada igual que ella misma y omitía dicho prefijo. Corregir la proyección y comprobarla contra una declaración NativeSharp realmente resuelta por `ProjectNameLookup` y el modelo de miembros.
- Alcance: `src/Interop/Cpp/CppSemanticFunction.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. La proyección permanece aislada, sin habilitar por ello el uso productivo de records.
- Evidencia 2026-11-04: la proyección emite ahora `type:<identidad>*`. La prueba obtiene `A::B::consume(C*)` desde una sesión real de Clang y compara su parámetro con la clave producida por `ProjectMemberModel::canonicalTypeKey` sobre `A.B.C*` resuelto por `ProjectNameLookup`; coinciden. Se ajustó también la prueba previa que asumía el formato incorrecto. Reconstrucción ON, suite de sesiones y `git diff --check`: código 0.
- Próximo paso: determinar dónde impedir uso por valor, desreferencia y operaciones que requieren layout para records importados antes de conectarlos al índice productivo. La igualdad de claves no demuestra seguridad ABI ni ejecución de llamadas.

## Comprobación de llamada con identidad nominal compartida

- Antes de activar records importados en el driver, comprobar si una firma obtenida de Clang que recibe `A::B::C*` llega hasta LLVM cuando una declaración controlada de NativeSharp proporciona la misma identidad nominal. Esto separa la resolución y generación de la llamada de la implementación pendiente de tipos importados; no supone equivalencia de layouts.
- Archivos: `tests/unit/CppClangSessionTests.cpp` y este registro. La prueba no usa ni modifica el driver ni declara terminada la interoperabilidad de objetos.
- Ampliación de verificación: emitir el objeto NativeSharp y enlazarlo con una implementación C++ real usando el ejecutor existente `pruebas-llamadas-escalares/ejecutar.cmd` y un fixture nuevo en esa misma carpeta. El ejecutable comprueba el resultado sin depender de layout de objeto.
- Evidencia 2026-11-04: Clang expuso `A::B::consume(C*)`; la firma proyectada se usó en una compilación NativeSharp con `struct C` declarado en el mismo namespace. La llamada superó análisis, MIR y verificación LLVM, conservó el símbolo externo de Clang y emitió objeto Win64. El ejecutor lo enlazó con una implementación C++ compilada con Clang y comprobó que un puntero nulo produce 37; también pasaron las demás llamadas del script. Construcción ON y suite de sesiones: código 0.
- Límite: el `struct` NativeSharp es un sustituto controlado para esta prueba; no se importó su declaración desde C++, no se compartió un objeto real y no se probó layout, construcción, destrucción ni acceso a campos. Próximo paso: conectar el registro de tipos importados a la compilación con validaciones explícitas de sus usos y comprobar ese camino sin la declaración sustituta.
- Extensión verificada 2026-11-04: `A::B::create()` devuelve desde C++ un puntero a una instancia real; NativeSharp lo pasa a `A::B::consume`, que confirma que es esa misma instancia. La ejecución combinó ese resultado con el caso nulo y obtuvo 3719, validado por el proceso. Construcción ON, suite de sesiones, enlace y ejecución de `pruebas-llamadas-escalares/ejecutar.cmd` y `git diff --check`: código 0. La instancia continúa siendo opaca para NativeSharp; su declaración sigue siendo un sustituto controlado, sin validar layout ni ciclo de vida.

## Inyección experimental de declarations de record

- Conectar una lista explícita de tipos C++ al lookup dentro de `ProjectCompilation`, solamente como interfaz experimental para comprobar la tubería sin declaración sustituta. El driver no la activa: falta impedir usos que requieran layout o vida de objetos; la inyección experimental no es soporte productivo.
- Alcance: `include/nativesharp/Frontend/ProjectCompilation.h`, `src/Frontend/ProjectCompilation.cpp`, `tests/unit/CppClangSessionTests.cpp`, prueba mixta ya existente y este registro. El tipo de prueba se obtiene de la sesión real de Clang; comprobar análisis, emisión y ejecución, y rechazar configuración posterior al inicio de compilación.
- Hallazgo al ejecutar: `A.B.C* p` se interpretó como expresión antes del lookup porque el parser solo conoce tipos detectados en fuentes NativeSharp. Para la vía experimental se añadirán las identidades importadas a las semillas del parser y de la clonación, sin activar esta ruta en el driver.
- Evidencia 2026-11-04: `setExperimentalCppImportedTypes` configura tipos antes de compilar y rechaza cambios posteriores; la fase de nombres los entrega a `ProjectNameLookup`. El parser y la observación clonada reciben el nombre y el nombre calificado como semillas. La prueba usa el record recuperado de Clang, elimina la declaración `struct C` de NativeSharp y compila hasta MIR y LLVM; el ejecutable enlazado con C++ pasa el puntero a la instancia real y comprueba 3719. Construcción ON, suite de sesiones, script de enlace y `git diff --check`: código 0.
- Restricciones: esta es una API experimental invocada explícitamente por la prueba; el driver no la usa. El frontend aún trata el record como struct sin layout real, por lo que no está habilitado para usuarios. Faltan la clasificación y validación de usos por valor, acceso, desreferencia, `sizeof`, aritmética de punteros, propiedad y lifetime, así como comprobar aliases, snapshots y semántica de las semillas del parser antes de conectar la importación automática.
- Siguiente comprobación: el cálculo existente de aliases del parser debe incluir los tipos C++ antes de resolver los aliases; se ajustará esa misma fase y se probará `using Alias = A.B.C` tanto en el parser principal como en la copia de observación.
- Evidencia 2026-11-04: `collectParserKnownTypeNames` incorpora los tipos experimentales antes de expandir aliases, y el parser normal y la copia usan el mismo conjunto. La prueba compila `using Alias = A.B.C; Alias* p`, clona la sintaxis y vuelve a enlazar y ejecutar el caso de puntero real sin declaración NativeSharp sustituta. Construcción ON, suite de sesiones, script de enlace y `git diff --check`: código 0. La activación automática sigue pendiente de barreras de seguridad y de comprobar caché y visibilidad entre proyectos.

## Identidad de caché frente a nombres C++

- El parser recibe nombres C++ que no figuraban en las claves de caché del corpus ni de sintaxis de módulos precompilados; un árbol parseado con otro conjunto de nombres podría reutilizarse incorrectamente. Incluir el conjunto ordenado de semillas experimentales en ambas claves, sin alterar la clave anterior cuando no existen tipos importados. Los productos publicados por el productor son independientes y no se sustituyen por la caché local.
- Alcance: `src/Frontend/ProjectCompilation.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. La prueba debe distinguir entradas locales con y sin tipos importados y comprobar reutilización estable con el mismo conjunto.
- Evidencia 2026-11-04: la clave local de sintaxis precompilada incluye las semillas C++ ordenadas y sin duplicados; sin tipos mantiene la clave anterior. Se incluyó también el conjunto en la clave del corpus, actualmente desactivada por `kTemplateParseCorpusCacheEnabled`. En una caché nueva, una compilación sin tipo y otra con el tipo crearon dos entradas; repetir la segunda conservó las dos entradas y sus marcas de tiempo, sin reescribirlas. Construcción ON, suite de sesiones, pruebas de llamadas y `git diff --check`: código 0.
- Límite: la prueba verifica la caché local, no la corrección de productos publicados por terceros con dependencias C++ propias. Las semillas siguen siendo globales al parser; falta verificar y resolver su efecto en la visibilidad de nombres entre proyectos antes de activar la vía experimental.

## Visibilidad de semillas de tipos C++

- El parser distingue `Token * p` entre multiplicación y declaración según los tipos conocidos. Una semilla importada desde un proyecto que no es visible no debe cambiar la interpretación del código en otro proyecto. Probar un proyecto consumidor con un tipo C++ `Token` y una dependencia NativeSharp que usa dos variables llamadas `Token` y `p`; comprobar a la vez que el consumidor sí pueda usar su tipo.
- Alcance: `src/Frontend/ProjectCompilation.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Aplicar a las semillas C++ la clausura de `ProjectReference` que usa el lookup; conservar el comportamiento existente de semillas NativeSharp. Verificar también la copia aislada de sintaxis.
- Evidencia 2026-11-04: las semillas C++ se calculan por proyecto según el cierre transitivo de referencias, tanto para el parser normal como para la copia aislada. Con un tipo `Token` propiedad de `App`, `Token * p` en su dependencia `Core` permanece como multiplicación mientras `Token* item` en `App` se reconoce como declaración; el caso inverso comprueba que `App` reconoce el tipo importado por `Core`. Construcciones ON y frontend OFF, suite de sesiones, llamadas mixtas y `git diff --check`: código 0.
- Límite: estas pruebas cubren referencias de proyecto y forma sintáctica; no establecen compatibilidad de layout, acceso ni vida de objetos y no activan la importación automática. La clave de caché conserva todas las semillas experimentales, lo que es una aproximación conservadora y puede provocar misses adicionales entre proyectos.

## Evitar layout NativeSharp inventado para records C++

- Hallazgo: el lookup experimental representa temporalmente un record C++ como declaración nominal sin miembros, pero `ProjectObjectLayoutModel::build` calcula por defecto un layout de struct NativeSharp vacío para ella. Ese resultado no corresponde necesariamente al layout de Clang y no debe usarse para `sizeof`, arreglos, campos por valor ni aritmética de punteros.
- Evitar la construcción anticipada de ese layout para tipos identificados como importados; si se requiere el tipo por valor, informar que su representación todavía no está disponible. Conservar el transporte de punteros. Alcance: `src/Frontend/ProjectObjectLayoutModel.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro; probar un campo NativeSharp por valor y el caso de punteros existente.
- Evidencia 2026-11-04: el modelo deja de crear un `StructValueLayout` vacío para tipos importados y devuelve layout desconocido en su consulta por clave. Una estructura NativeSharp con un campo C++ por valor recibe `IncompleteType` con diagnóstico explícito; la compilación y ejecución del intercambio de punteros nulos y reales sigue funcionando. Construcción ON, suite de sesiones, llamadas mixtas y `git diff --check`: código 0.
- Siguiente paso: transportar tamaño y alineación de Clang para definiciones completas y proporcionar una representación que respete el ABI, sin concluir que disponer de tamaño basta para habilitar construcción, copia, destrucción o acceso a campos. Quedan por examinar las demás operaciones que necesitan layout y los tipos incompletos.

## Procedencia del tamaño y alineación C++

- Transportar el tamaño y la alineación de una definición única de Clang hasta el registro de tipos del lookup; conservar como desconocidos los tipos incompletos, conflictivos o con varias definiciones no verificadas. No usar aún estos datos como layout completo de NativeSharp ni habilitar por valor, construcción o acceso a campos.
- Archivos previstos: `include/nativesharp/Frontend/ProjectNameLookup.h`, `src/Frontend/ProjectNameLookup.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Comprobar que los datos de Clang sobrevivan al registro y que conflictos e incompletos no inventen un layout.
- Evidencia 2026-11-04: una definición única y completa aporta tamaño y alineación en bytes a `CppImportedTypeDeclaration`, verificando que sean representables y coherentes; `ProjectNameLookup::cppImportedType` conserva esos datos por ordinal. Las pruebas comparan ambas dimensiones con la sesión Clang real, y verifican que conflictos, tipos adelantados y definiciones múltiples aún no verificadas no reciban dimensiones. La primera construcción pasó con advertencias por inicialización omitida del nuevo miembro; se añadió su valor predeterminado y la reconstrucción final terminó sin esas advertencias. Suite de sesiones, llamadas mixtas y `git diff --check`: código 0.
- Límite: las dimensiones se conservan pero aún no se usan como layout del objeto ni habilitan `sizeof`, aritmética de punteros, campos, ABI por valor o ciclo de vida. Próximo paso: comprobar e integrar su uso en consultas de tamaño y paso de punteros sin permitir todavía operaciones de objeto que dependen de miembros y lifetime.

## Consultas de tamaño y paso de puntero para C++

- Conectar tamaño y alineación importados a la consulta física usada por `sizeof`, `alignof` y paso de punteros, manteniendo el bloqueo separado de layout de campos por valor. Validar dimensiones proporcionadas al lookup para no usar tamaños o alineaciones inválidos.
- Alcance: `src/Frontend/ProjectNameLookup.cpp`, `src/Frontend/ProjectObjectLayoutModel.cpp`, `tests/unit/CppClangSessionTests.cpp`, el fixture `pruebas-llamadas-escalares/cpp-record-pointer-externo.cpp` y este registro. El fixture C++ tendrá un arreglo real de dos objetos con padding; NativeSharp deberá medir el tipo y pasar el segundo elemento calculado a C++ para verificar su dirección. La integración sigue experimental y no activa el driver.
- Evidencia 2026-11-04: `ProjectNameLookup` rechaza dimensiones nulas, alineaciones que no son potencia de dos y tamaños incompatibles; la consulta física devuelve las dimensiones conservadas de Clang sin crear un `StructValueLayout` NativeSharp. La prueba usa `C { long long data; int other; }`, comprueba `sizeof(C) == 16` y `alignof(C) == 8`, y C++ confirma que `p + 1` desde NativeSharp apunta exactamente al segundo objeto de su arreglo. También se comprueba que un campo C++ por valor siga rechazándose. Construcción ON, suite de sesiones, enlace y ejecución, y `git diff --check`: código 0. Se corrigió una advertencia del `static_assert` del fixture y se repitió el enlace sin advertencias.
- Límites: estas dimensiones no constituyen implementación de campos, ABI de paso por valor, construcción, copia, movimiento ni destrucción. La vía sigue requiriendo inyección experimental; faltan las demás barreras de uso y la conexión al driver. Próximo paso: comprobar los casos de tipo incompleto y de representación insuficiente en `sizeof`, `alignof` y aritmética para garantizar que nunca recurran a un tamaño de reserva inventado.

## Consultas sobre tipo C++ incompleto

- Comprobar con una declaración adelantada obtenida de Clang que un puntero pueda transportarse, pero que `sizeof`, `alignof`, indexación y aritmética no produzcan un tamaño supuesto. Utilizar la misma ruta experimental de compilación y comprobar errores y controles positivos.
- Alcance inicial: `tests/unit/CppClangSessionTests.cpp` y este registro; modificar la fase que resulte responsable únicamente si una prueba demuestra un cálculo incorrecto.
- Hallazgo: una variable local `Missing.C value` llegó a MIR aun sin tamaño ni semántica de objeto; se agregará la validación en `src/Frontend/Bodies/StatementAnalyzer.cpp` para una declaración local de record C++ por valor, mientras no exista la implementación de su ciclo de vida. El control con punteros permanece permitido. Este guard no pretende cubrir arreglos, parámetros ni todos los demás usos por valor.
- Evidencia 2026-11-04: con `struct C;` leído por Clang, el control que transporta `C*` llega a MIR; `sizeof`, `alignof`, `p + 1`, `p += 1` y `&p[1]` se rechazan por ausencia de layout. La variable `C value` inicialmente compilaba sin error; ahora se rechaza en análisis de cuerpos con `IncompleteType` y un mensaje que identifica la falta de soporte de lifetime por valor. La primera prueba usó `get`, palabra reservada en NativeSharp, y se corrigió a `fetch`; las comprobaciones negativas exigen ahora el error de layout correspondiente, no cualquier error. Construcción ON, suite de sesiones, llamadas mixtas y `git diff --check`: código 0.
- Límite: el guard afecta a locales nominales directos de C++ y no demuestra cobertura de arreglos, parámetros, retornos, agregados, `new`, `stackalloc`, ni desreferencia. Próximo paso: seguir esas rutas y establecer explícitamente sus validaciones o implementar la semántica real de objeto correspondiente.

## Variables globales C y C++: inventario y proyección

- Preparar el acceso directo a variables externas conservando proyecto, sesión, nombre, símbolo de Clang y calificador. La ruta actual de estados NativeSharp usa accesores de inicialización propios y no puede asumirse equivalente a un símbolo global C++; TLS y los calificadores requieren su semántica explícita.
- Primera modificación: inventariar todas las declaraciones globales por sesión y proyectar de forma explícita variables escalares externas mutables no volátiles; conservar los demás casos con el motivo de falta de soporte, sin activarlos en el driver. Archivos: `include/nativesharp/Interop/CppSemanticInventory.h`, `src/Interop/Cpp/CppSemanticInventory.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. La meta posterior es lectura y escritura enlazadas directamente, sin exigir un accesor NativeSharp.
- Evidencia 2026-11-04: `collectCppSemanticGlobals` conserva proyecto, sesión, triple y declaración de cada global; `projectCppSemanticGlobal` conserva tipo canónico y símbolo exacto para escalares externos mutables. Sesiones reales de C y C++ comprobaron `c_global` y `?shared@feature@@3HA` en proyectos distintos; variables internas, `const`, `volatile` y TLS quedaron con rechazo explícito, no descartadas. Construcción ON, suite de sesiones, llamadas mixtas y `git diff --check`: código 0.
- Límite: el inventario todavía no hace accesibles las variables a código NativeSharp. `const`, `volatile`, TLS, punteros y demás tipos siguen pendientes como capacidades, no como exclusiones del objetivo. Próximo paso: integrar el símbolo proyectado en el modelo de estados y en MIR/LLVM como almacenamiento externo real, evitando el accesor de inicialización de NativeSharp, y probar lectura y escritura enlazadas.
- Se añadió una entrada experimental de estados externos y su propagación a MIR/LLVM, con declaración del símbolo de datos de Clang y sin el accesor NativeSharp. La identidad de caché LLVM incluye esta distinción. La suite ON compiló y verificó el IR para `?cpp_global@tools@@3HA`; un ejecutable mixto enlazó la definición C++ y comprobó que NativeSharp escribe y lee 41 y que C++ observa 41. La suite de llamadas mixtas terminó con código 0.
- La API experimental también rechazó entradas sin símbolo, entradas duplicadas y cambios una vez iniciada la compilación; reconstrucción incremental, suite y enlace mixto posteriores terminaron con código 0. `git diff --check` no detectó errores.
- Límite de esta evidencia: el símbolo se inyecta mediante API experimental interna; aún no se conecta el inventario al driver ni se resuelven declaraciones repetidas, conflictos, accesibilidad entre proyectos, `const`, `volatile`, TLS, DLL o variables de tipo no escalar. El resultado no significa que estas capacidades estén completas.
- Siguiente cambio: seleccionar por proyecto y nombre las declaraciones externas del inventario, conservando sus índices y comprobando también los alias del símbolo enlazado. Comparar triple, tipo canónico, tamaño, alineación y calificadores antes de unir redeclaraciones; preservar explícitamente declaraciones no proyectadas y conflictos. Comprobar repetidos, diferencias de tipo, alias incompatibles, proyectos y almacenamiento interno con sesiones de Clang. Archivos: `include/nativesharp/Interop/CppSemanticInventory.h`, `src/Interop/Cpp/CppSemanticInventory.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Este selector prepara la conexión automática; no activa por sí solo el driver.
- Evidencia 2026-11-04: `selectCppSemanticGlobal` conserva índices de declaraciones, alias, internos y no proyectados; sólo proporciona un candidato si coinciden las declaraciones externas y sus alias. Se partió de un inventario de sesiones Clang reales y se duplicaron o alteraron copias para reproducir redeclaraciones idénticas, conflicto de tipo, alias `volatile` incompatible y triple diferente; también se comprobó separación por proyecto y exclusión del símbolo interno. La construcción ON, `nsc_cpp_session_tests.exe` y `git diff --check` terminaron con código 0.
- Pendiente: no se ha conectado el selector al driver ni demostrado su uso con DLL; la prueba de conflictos usa copias alteradas de declaraciones reales, no headers conflictivos independientes. El selector recorre el inventario por consulta; evaluar un índice antes de emplearlo masivamente. Las declaraciones no proyectadas y los conflictos deben diagnosticarse cuando corresponda semánticamente, sin fallar por nombres sombreados que no se utilicen.
- Próximo cambio: permitir que el modelo experimental conserve un estado C/C++ con motivo de proyección pendiente y que `analyzeNamespaceState` emita el error únicamente cuando ese estado resulte seleccionado. Un local o parámetro que lo sombree debe conservar prioridad. El estado no proyectado no debe generar almacenamiento MIR. Archivos: `NamespaceStateSymbol.h`, `ProjectNamespaceFunctionModel.cpp`, `IdentifierAnalyzer.cpp`, `MirBuildCoordinator.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro. Esto prepara la integración automática; no altera aún el driver ni resuelve colisiones con declaraciones NativeSharp del mismo namespace.
- Evidencia 2026-11-04: el motivo se conserva en `cppImportError`, el análisis lo informa al seleccionar el estado y MIR omite sus campos de almacenamiento. La prueba usa cuatro programas: un local sombrea el nombre, un parámetro lo sombrea, el programa accede a la variable pendiente y otro no la usa. Los dos primeros y el último compilan; el acceso produce el diagnóstico específico de variable `volatile` no proyectada. Construcción ON, suite de sesiones, enlace y ejecución de llamadas mixtas, y `git diff --check`: código 0.
- Límite: estos casos inyectan el estado experimentalmente. No se probaron aún sombra por campos de clase, nombres calificados ni colisiones con una variable de namespace NativeSharp del mismo proyecto; la conexión automática al driver permanece pendiente.
- Siguiente cambio: conservar un estado C/C++ y uno NativeSharp del mismo proyecto con el mismo nombre como candidatos separados, en vez de rechazar anticipadamente todo el proyecto; si el nombre llega a usarse, el análisis existente debe diagnosticar ambigüedad. La búsqueda de referencia de comprobación de índices debe examinar ambos estados. Verificar nombre usado, local que lo sombrea y nombre sin usar. Archivos: `ProjectNamespaceFunctionModel.cpp`, `tests/unit/CppClangSessionTests.cpp` y este registro.
- Evidencia 2026-11-04: el índice por namespace conserva ambas declaraciones, mientras `stateByName_` mantiene el primer estado para sus consultas individuales; la comprobación de referencia examina todos los estados. Los programas con la colisión sin usar y con un local que la sombrea llegaron a MIR; el acceso calificado a la colisión produjo `ambiguous namespace state reference`. Construcción ON, suite de sesiones, llamadas mixtas y `git diff --check`: código 0.
- La prueba histórica `nsc_namespace_state_model_tests` no está disponible en esta configuración: se intentó construir ese target y Ninja indicó que no existe; `CMakeCache.txt` registra `NSC_BUILD_TESTS=OFF`. No se ha afirmado que esa suite haya pasado. El caso de colisión se verificó en la suite de interoperabilidad, pero aún falta probar campos de clase, múltiples proyectos y la activación desde el driver.
- Próximo cambio: conectar las sesiones C/C++ del proyecto con un proveedor de estados por demanda sintáctica. Utilizar el observador ya existente de nombres y sus proyectos visibles; agrupar y deduplicar las declaraciones externas seleccionadas; pasar al modelo tanto los estados escalares admitidos como los motivos diferidos de conflictos o tipos pendientes. Activar el proveedor únicamente cuando el proyecto tenga sesiones C/C++; conservar la inyección experimental para pruebas. Archivos previstos: `ProjectCompilation.h`, `ProjectCompilation.cpp`, `ProjectCompilationSnapshots.cpp`, `CppSemanticInventory.h/.cpp`, `PrecompiledModuleConfiguration.cpp`, pruebas de sesiones y `pruebas-proyecto/`, además de este registro. Verificar compilación y ejecución desde `.nsproj`, rechazo diferido y regresión de proyectos existentes.
- Evidencia 2026-11-04: el driver instala `provideCppSemanticGlobals` cuando hay sesiones de proyecto; el observador suministra nombres y proyectos visibles, el proveedor deduplica demandas y transfiere estados escalares o motivos diferidos. Un índice de nombres evita recorrer todas las declaraciones para cada identificador que no exista; las selecciones coincidentes todavía recorren el inventario. `nsc.exe` se construyó en A: y superó su comprobación de runtime estático.
- El proyecto C real escribió `project_shared=5`, lo leyó en NativeSharp y mediante `project_read_shared()` en C y devolvió 13. El proyecto C++ escribió `tools.shared=7`, lo leyó en NativeSharp y mediante `tools.read_shared()` y devolvió 50. Ambos se construyeron desde `.nsproj` con `CppLibraryReference` y bibliotecas estáticas; el segundo comprobó además el diagnóstico al acceder a `tools.flagged` de tipo `volatile`. La prueba de proveedor verifica deduplicación, visibilidad por proyecto y conservación del motivo pendiente.
- La primera ejecución de la suite falló porque su proyecto de referencia utiliza el `Main.ns` modificado y configuraba manualmente sólo el proveedor de funciones. Se actualizó esa configuración de prueba para instalar el proveedor de globales; la reconstrucción y la repetición de `nsc_cpp_session_tests.exe` terminaron con código 0. Las dos pruebas de proyecto, las llamadas mixtas y `git diff --check` también terminaron con código 0 tras la reconstrucción final.
- Límite: quedan pendientes `const`, `volatile`, TLS, punteros y otros tipos de variable, DLL, resolución y visibilidad completa entre proyectos, campos estáticos y medición con headers grandes. Un diagnóstico de capacidad pendiente no es interoperabilidad completa. Las colisiones sintácticas y la selección semántica necesitan más pruebas, incluidas clases y namespaces anidados.

## Variables escalares externas volatile

- El backend ya marca cargas y escrituras según `volatileAccess`, pero el estado de namespace importado no conserva ese indicador. Proyectar el calificador Clang hasta `NamespaceStateSymbol`, HIR y MIR; comprobar lecturas, escrituras y asignaciones compuestas mediante IR LLVM y ejecución. `volatile` no implica atomicidad ni sincronización entre hilos.
- Mientras no exista un tipo de puntero que conserve el calificador, rechazar explícitamente la toma de dirección de estas variables importadas: pasar su dirección como puntero no calificado podría generar accesos no volátiles. Mantener fuera de esta restricción las variables NativeSharp existentes y conservar la asignación directa.
- Alcance: `CppSemanticInventory.h/.cpp`, `NamespaceStateSymbol.h`, `IdentifierAnalyzer.cpp`, `HirNameExpressionLowering.cpp`, `MirBuildCoordinator.cpp`, `tests/unit/CppClangSessionTests.cpp`, `pruebas-proyecto/` y este registro. Usar `const` aún no soportado para seguir comprobando diagnósticos diferidos del driver.
- Evidencia 2026-11-04: el calificador extraído por Clang se conserva en la proyección, el estado, HIR y MIR. Para `tools.cpp_volatile = 5; tools.cpp_volatile += 2; return tools.cpp_volatile`, LLVM verificó al menos dos `load volatile i32` y dos `store volatile i32`. La toma de dirección y la referencia local sin calificador se rechazaron en la suite y en un proyecto C++ real.
- El proyecto C importó `project_flag`, escribió 6, sumó 2 y obtuvo 8 tanto desde NativeSharp como desde la función C; el programa devolvió 29. El proyecto C++ hizo lo mismo con `tools.flagged` y valor final 6; devolvió 62. La prueba de rechazo diferido usa ahora una global `const`. Ambos proyectos, la suite de sesiones, llamadas mixtas y `git diff --check` terminaron con código 0; `nsc.exe` se construyó en A: y pasó su comprobación de runtime estático.
- La primera ejecución de la suite falló porque el test esperaba un texto distinto al diagnóstico real de referencia local; se comprobó este diagnóstico con el driver, se corrigió la expectativa y se repitió la suite. Posteriormente se reforzó la verificación de lecturas y escrituras volátiles y volvió a pasar.
- Límites: no se han demostrado todas las operaciones sobre estos valores, ni punteros y referencias calificados, `const volatile`, TLS, bibliotecas dinámicas o todos los objetivos y tipos. La falta temporal de punteros/referencias calificados no es cierre de la interoperabilidad solicitada.

## Variables escalares externas const

- Una global `const` de C/C++ es almacenamiento definido por la biblioteca: no debe confundirse con una constante NativeSharp evaluada durante compilación ni recibir un inicializador inventado. Conservar el calificador como una propiedad de solo lectura para el análisis semántico y emitir la misma referencia externa a datos; combinarlo con `volatile` cuando corresponda.
- Verificar lecturas desde bibliotecas C y C++, escrituras rechazadas, ausencia de almacenamiento duplicado y accesos `const volatile` marcados como volátiles. Hasta admitir punteros o referencias calificados, rechazar la toma de dirección y referencias ordinarias de estos datos para impedir que se elimine `const`.
- Alcance: `CppSemanticInventory.h/.cpp`, `NamespaceStateSymbol.h`, `IdentifierAnalyzer.cpp`, `MirBuildCoordinator.cpp`, `tests/unit/CppClangSessionTests.cpp`, `pruebas-proyecto/` y este registro. Usar TLS no soportado para conservar la prueba de diagnóstico diferido del driver.
- Evidencia 2026-11-04: la proyección de Clang conserva `isConst` e `isVolatile`; `isCppReadonly` impide escritura y no activa el mecanismo de constantes plegadas de NativeSharp. LLVM verifica que ambas globales C++ de prueba se declaran como `external global i32`, sin inicializador propio, y que la lectura de `const volatile` es `load volatile i32`. La suite rechaza escritura, puntero y referencia ordinaria hacia la global `const`.
- Desde proyectos `.nsproj`, el programa C leyó `project_limit=9` y `project_status=4` y devolvió 42; el C++ leyó `tools.limit=11` y `tools.status=3` y devolvió 76. El proyecto C++ comprobó diagnósticos de escritura a `const`, dirección sin calificador y uso pendiente de TLS, además de las restricciones previas de `volatile`. La construcción ON, ambas ejecuciones, la suite de sesiones, llamadas mixtas y `git diff --check` finalizaron con código 0. El compilador construido en A: pasó su comprobación de runtime estático.
- Límites: la lectura externa no convierte la variable en una expresión constante de compilación; no se ha demostrado importación de inicializadores `constexpr`. Punteros y referencias calificados, TLS, DLL, inicialización dinámica, tipos no escalares y objetivos adicionales siguen pendientes. El soporte demostrado se limita a escalares con almacenamiento externo y a los casos indicados.

## Globales externas con punteros a escalares

- Proyectar punteros Windows x64 a escalares o `void`, incluidos niveles múltiples, conservando `const` y `volatile` del puntero global. No eliminar calificadores del objeto apuntado ni de niveles internos: esos casos continúan con diagnóstico diferido hasta contar con sus contratos completos.
- Verificar acceso directo al símbolo y ejecución a través de `.nsproj` con bibliotecas C y C++; comprobar lectura, reasignación y desreferencia, un puntero `const` cuyo objeto apuntado sigue siendo mutable, un puntero `volatile` y rechazo de `const` en el objeto apuntado. Mantener los demás comportamientos y diagnósticos previos.
- Archivos: `src/Interop/Cpp/CppSemanticInventory.cpp`, `tests/unit/CppClangSessionTests.cpp`, `pruebas-proyecto/` y este registro. El tipo NativeSharp del estado usa las claves de puntero existentes; se conserva por separado la identidad canónica de Clang.
- Evidencia 2026-11-04: Clang proyectó `int*`, `int**`, `void*`, `int* const` e `int* volatile` con profundidad y calificadores superiores; `const int*` e `int* const*` permanecen sin proyectar en esta ruta. El IR LLVM verificado de una asignación y lectura de puntero `volatile` contiene `store volatile ptr` y `load volatile ptr`.
- En proyectos reales, NativeSharp modificó el entero apuntado, asignó `null`, restauró el puntero desde una global `int* const` y modificó el mismo entero mediante los punteros `const` y `volatile`. La función de la biblioteca observó 15; los programas C y C++ devolvieron respectivamente 72 y 106. El proyecto C++ rechazó tanto la reasignación de `int* const` como el uso pendiente de `const int*`, sin retirar sus calificadores. Construcción ON, suite de sesiones, ambas ejecuciones, llamadas mixtas y `git diff --check`: código 0.
- Límites: `int**` y `void*` se comprobaron en proyección, no en ejecución; faltan punteros a objetos, funciones, arreglos y calificadores internos, condiciones de validez/lifetime, DLL y ejecución en otros targets. `volatile` del puntero no vuelve volátil al dato apuntado. No se ha resuelto todavía el contrato de punteros a datos `const` o `volatile`.

## Globales con puntero a dato escalar const

- Proyectar `const T*` para escalares de un nivel conservando por separado la inmutabilidad del dato apuntado y los calificadores del puntero global. El análisis existente debe permitir leer el dato y reasignar el puntero mutable, pero rechazar modificar el dato o convertirlo implícitamente a `T*`.
- No admitir aún `volatile` en el dato apuntado ni calificadores en niveles internos. Rechazar la dirección o referencia de la global `const T*`, ya que el tipo resultante tendría un calificador anidado que el modelo actual no conserva. Comprobar conversiones, escritura y dirección con diagnósticos explícitos; ejecutar proyectos C y C++ y preservar las pruebas previas.
- Alcance: `CppSemanticInventory.h/.cpp`, `NamespaceStateSymbol.h`, `IdentifierAnalyzer.cpp`, `tests/unit/CppClangSessionTests.cpp`, `pruebas-proyecto/` y este registro.
- Evidencia 2026-11-04: `const int*` se proyecta como puntero a `int` con `pointerPointeeReadonly`; el análisis conserva esa propiedad para desreferencias, inicialización, argumentos y asignaciones. La suite verificó lectura mediante `int const*`, reasignación de la global a `null`, rechazo de escritura al dato, conversión implícita a `int*` y toma de dirección de la global. `volatile int*` y `int* const*` continúan rechazados como tipos pendientes.
- En proyectos reales se leyeron 13 desde la biblioteca C y 21 desde la C++; los programas devolvieron 85 y 148. En el caso C++, NativeSharp guardó, anuló y restauró el puntero y lo pasó a una función que recibe `const int*`; pasar el mismo valor a una función que requiere `int*`, eliminar `const` en un local, escribir el dato y tomar la dirección de la global produjeron los diagnósticos esperados. Construcción ON, suite de sesiones, ambas ejecuciones, llamadas mixtas y `git diff --check`: código 0. El `nsc.exe` construido en A: pasó su comprobación de runtime estático.
- Límites: este caso cubre un nivel y datos escalares; no habilita `volatile` en el objeto apuntado ni calificadores anidados, registros, funciones, DLL o valores que deban tratarse como expresiones constantes. La dirección de la propia global sigue rechazada hasta poder conservar los calificadores de varios niveles.
- Siguiente verificación: con el proyecto C++ real, intentar escribir mediante aritmética e indexación de `const int*`, y asignar a `int*` el resultado de un condicional y de una coalescencia que incluyen ese puntero. Registrar el diagnóstico exacto y corregir la propagación únicamente si alguna forma elimina silenciosamente `const`. Archivos inicialmente autorizados: `pruebas-proyecto/`, este registro y, si la reproducción identifica el punto responsable, ese punto del análisis semántico con sus pruebas dirigidas.
- Evidencia previa: con `nsc` construido en A: y el runtime/enlazador de la prueba, `*(tools.readonly_pointee + 0) = 1` y la conversión del condicional `?:` a `int*` se rechazaron; `tools.readonly_pointee[0] = 1` y `int* p = tools.readonly_pointee ?? tools.pointer` compilaron incorrectamente. La primera ejecución sin configurar el runtime llegó al enlazado y produjo un error ajeno; se corrigió el entorno y se obtuvo la reproducción indicada.
- Causa identificada: `analyzeRawIndex` no consulta `base.pointerPointeeReadonly`; `analyzeExpressionKindNullCoalescingExpression` fija el tipo pero no combina los contratos de puntero de ambas ramas. Corregir estas dos rutas, conservar las demás reglas de índice y coalescencia y añadir controles de lectura y punteros mutables. Archivos: `IndexAndRangeExpressions.cpp`, `OperatorAndConditionalExpressions.cpp`, `pruebas-proyecto/`, pruebas de sesiones si hacen falta y este registro.
- Verificación adicional: la protección de dirección de la global se aplicaba también mientras se analizaba el receptor en `&tools.readonly_pointee[0]`, aunque se toma la dirección del elemento, no la de la global. El resultado era seguro pero rechazaba una forma válida de obtener `const int*`. Acotar el diagnóstico al operando directo de `&`, conservando la condición de no direccionable de la propia global y la bandera existente para límites de arrays. Archivos adicionales: `ProjectMethodBodyModel.h` e `IdentifierAnalyzer.cpp`; verificar `int const* p = &tools.readonly_pointee[0]` y rechazo de conversión a `int*` y de `&tools.readonly_pointee`.
- Evidencia 2026-11-05: la indexación de puntero conserva la inmutabilidad del dato; `??` combina de forma conservadora mutabilidad, nulabilidad, procedencia, alineación, espacio de direcciones y condición de un-paso-después. El diagnóstico de dirección distingue el operando directo de los receptores internos. `inspeccionar-const-expresiones.cmd` comprobó escritura rechazada por aritmética e índice, conversiones rechazadas mediante `?:`, `??` en ambas posiciones y con `null`, y conversión rechazada de la dirección del elemento; los controles de lectura por índice, escritura mediante puntero mutable, coalescencia de solo lectura y dirección del elemento como `int const*` se ejecutaron correctamente.
- El primer intento del script ampliado no encontró una etiqueta por sus finales de línea; se normalizaron a CRLF y la repetición terminó con código 0. Construcción ON, suite de sesiones, proyectos C y C++ (85 y 148), llamadas mixtas y `git diff --check`: código 0. El comportamiento de calificadores anidados y los demás tipos todavía no queda demostrado por estas pruebas.

## Análisis previo a la importación automática de tipos C++

- Objetivo: comprobar si incorporar nombres de tipos C++ al parser cambia una expresión NativeSharp existente en el mismo proyecto. En particular, comparar `Token * p;` con y sin una declaración importada `Shared::Token` visible mediante `using Shared`.
- Alcance de la comprobación: `tests/unit/CppClangSessionTests.cpp` y este registro. Es una prueba de la infraestructura experimental; no habilita aún la importación automática ni modifica el parser. El resto de los proyectos, los compiladores desplegados y sus rutas permanecen protegidos.
- Criterio: inspeccionar el árbol sintáctico y los diagnósticos en ambos casos; registrar la evidencia antes de escoger cómo conectar los tipos al driver. No inferir que los conflictos de nombres entre proyectos resuelven la ambigüedad dentro del proyecto propietario.
- Evidencia 2026-11-05: `nsc_cpp_session_tests` construyó correctamente. Con la misma fuente, el árbol contiene la multiplicación sin el tipo importado (`plain=1`) y deja de contenerla al inyectar `Shared::Token` (`imported=0`); la nueva aserción falla deliberadamente y la suite termina con código 1. El análisis productivo todavía no activa esta inyección. `Parser::isKnownTypeName` consulta nombres conocidos, y `ParseContinuationCoordinator::declarationStatementStarts` usa `looksLikeVariableDeclaration` para seleccionar declaración antes que expresión. Estas ubicaciones explican la ruta candidata; todavía hay que determinar las reglas de resolución de nombres y ámbitos antes de modificarla. La prueba queda abierta y no debe interpretarse como una regresión ya corregida.
- Control adicional: declarar `Shared.Token` en NativeSharp, en lugar de inyectarlo desde C++, también produce `native=0`. Por tanto, esta desambiguación ya ocurre con tipos NativeSharp; no hay evidencia suficiente para atribuirla a C++ ni cambiar esa regla del lenguaje. Se ajusta la aserción para comprobar que la proyección se comporta igual que un tipo NativeSharp y se mantiene como riesgo abierto que introducir un nombre visible pueda cambiar el análisis de una fuente previa. Falta comprobar los diagnósticos y reglas semánticas de la colisión antes de decidir la integración productiva.
- La suite reconstruida pasó con código 0 y mostró `plain=1 imported=0 native=0`. En el caso nativo, `nsc --check-project-method-bodies` informa `duplicate local/parameter name 'p' in the same method scope`: el parser interpreta `Token * p` como una segunda declaración. Esto comprueba el diagnóstico en el control NativeSharp, pero no determina todavía la política deseada para nuevos nombres C++ ni valida la importación automática.
- Siguiente caso: el mismo proyecto sin `using Shared`, con `Shared.Token` disponible solamente por su nombre calificado. Comprobar si la semilla C++ altera igualmente `Token * p;`, pese a que el nombre corto no se ha importado. Alcance inicial: `tests/unit/CppClangSessionTests.cpp` y este registro; no modificar el parser hasta demostrar el efecto.
- Evidencia 2026-11-05: la prueba mostró `unqualified name without using: plain=1 imported=0` y terminó con código 1. `collectParserKnownTypeNames` inserta incondicionalmente `imported.identity.name()` para todo tipo C++ accesible desde el proyecto, incluso si su namespace no es visible en el archivo. La prueba conserva el fallo abierto. La corrección debe respetar namespace, `using`, alias, ámbitos anidados y referencias entre proyectos; un filtro global aproximado puede ocultar tipos válidos o seguir afectando archivos ajenos. Antes de conectar el driver, estudiar la resolución por ámbito o una representación que permita resolver la ambigüedad semánticamente. El parser productivo todavía no recibe estas semillas automáticamente.
- Ruta investigada: `ProjectNameLookup::bindScope` en `src/Frontend/NameLookup/QualifiedNamespaces.cpp` hereda un entorno por ámbito, incorpora directivas del ámbito y usa `qualifiedNameLadder` para buscar tipos; `Parser::isKnownTypeName` en `src/Parser/TypeParser.cpp` solo recibe un conjunto compartido de nombres, sin contexto de namespace. `parserTypeIndexesByProject` usa ese conjunto para todas las unidades visibles del proyecto, y `cloneSyntaxForObservation` vuelve a usarlo. Siguiente acción: estudiar cómo proporcionar al parser una consulta por unidad y posición con las mismas reglas de visibilidad, incluyendo sintaxis de namespace por archivo y anidado, directivas `using` y alias, sin alterar las semillas NativeSharp ni la caché; mantener la nueva prueba roja hasta verificar la corrección.
- Ampliación del caso reproducido: comprobar simultáneamente que `Shared.Token* item` sigue interpretándose como declaración en el archivo sin `using`. Así la corrección no podrá consistir en retirar toda la información del tipo C++ del parser. Se modifica únicamente la prueba experimental y esta bitácora; el comportamiento productivo sigue intacto.
- Resultado 2026-11-05: reconstrucción ON exitosa; la suite muestra `plain=1 imported=0 qualified=1` y termina con código 1 por la interferencia ya identificada. El nombre completo sí se reconoce como declaración; la prueba mantiene ambos requisitos separados. `ProjectSyntaxUnit::parse` construye un parser por unidad a partir de un conjunto compartido, mientras `ProjectNameLookup::bindScope` construye un entorno por ámbito: el mecanismo de consulta debe llevar contexto específico de unidad y posición, no solo filtrar todo el proyecto por el texto de un `using`. Siguiente acción: diseñar y comprobar el contexto léxico de namespaces y alias antes de conectarlo a `isKnownTypeName`, tomando en cuenta también `cloneSyntaxForObservation` y los productos sintácticos precompilados.
- Preparación del mecanismo: introducir una representación inmutable de nombres conocidos por intervalos de ubicación de origen, con identidad de archivo y consulta que admite ámbitos anidados sin filtrar a hermanos. Alcance `include/nativesharp/Parser/ScopedKnownTypeNames.h`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Esta pieza no se conecta aún al parser: falta construir sus ámbitos desde directivas y namespaces con la misma resolución que `ProjectNameLookup`. Los nombres nativos y las cachés permanecen sin cambios. Comprobar límites, anidamiento, archivos distintos y ausencias; la prueba de interferencia permanece roja hasta completar la conexión.
- Evidencia 2026-11-05: compilación de `nsc_cpp_session_tests` exitosa. La aserción de intervalos, anidamiento, hermanos, otros archivos y límites no produjo fallo; la suite todavía termina con código 1 exclusivamente por la prueba previa de interferencia (`plain=1 imported=0 qualified=1`). La consulta actual recorre todos los ámbitos, O(n) por consulta; no se ha evaluado su coste en proyectos grandes y no está conectada a producción. Antes de usarla es necesario construir intervalos correctos por unidad, aplicar la resolución canónica de `using` y alias, conectar el parser y comprobar la complejidad; no confundir la estructura preparada con la corrección terminada.
- Conexión controlada: permitir que `ProjectSyntaxUnit::parse` entregue opcionalmente un índice de ámbitos al parser y que `isKnownTypeName` consulte la ubicación del token actual, incluso en un parser hijo. Archivos: `Parser.h`, `ParserContext.cpp`, `TypeParser.cpp`, `ProjectSyntaxUnit.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Sin índice la ruta previa debe ser idéntica. Probar con ámbitos proporcionados explícitamente: una declaración de puntero en un intervalo y una multiplicación en un ámbito hermano. El productor automático de índices, alias y la caché siguen pendientes; no activar en el driver.
- Evidencia 2026-11-05: reconstrucción ON de 312 acciones completada. La prueba con `ProjectSyntaxUnit` real, índice explícito, `Item* p` en `Left` y `Item * p` en `Right` no reportó fallo; `git diff --check` pasó. La suite continúa en código 1 por la regresión previa de la inyección global, que sigue mostrando `plain=1 imported=0 qualified=1`. El índice se consulta usando la ubicación del parser hijo y se transmite desde la unidad solo cuando se proporciona. Falta producirlo a partir de namespaces y directivas, cambiar el sembrado C++ global y verificar la ruta clonada y la caché; no se ha habilitado automáticamente ni probado comportamiento productivo con esta vía.
- Construcción de ámbitos: crear un recorrido léxico de cada unidad que conserve límites, padres, nombres de namespace y directivas `using`, incluidas formas anidadas y de archivo. No interpreta `using` locales dentro de cuerpos ni aplica todavía resolución semántica. Archivos: `include/nativesharp/Parser/LexicalNamespaceScopes.h`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Verificar intervalos, padres, namespaces con puntos, directivas y exclusión de cuerpos. No activar en compilación productiva hasta enlazar la resolución canónica y el aislamiento por proyecto.
- Evidencia 2026-11-05: la reconstrucción ON terminó con código 0; las pruebas del recorrido de namespaces anidados y con puntos, `using` y alias, exclusión de `using` dentro de una función y namespace de archivo no reportaron fallo. `git diff --check` pasó. La suite general conserva código 1 por la interferencia de la inyección global (`plain=1 imported=0 qualified=1`). El inventario léxico aún no resuelve nombres ni está conectado al productor de índices; actualmente registra targets formados por identificadores calificados simples y deja otros tipos de alias para ampliar. Siguiente acción: resolver las directivas con las mismas prioridades de `ProjectNameLookup::resolveNamespaceTarget`, incluyendo namespaces visibles nativos y C++, y generar las semillas acotadas antes de sustituir las globales.
- Primera conexión de resolución: extraer la búsqueda textual de namespace relativo a global a un helper compartido por `ProjectNameLookup` y el futuro productor de ámbitos, conservando en cada consumidor su propio criterio de visibilidad. Alcance: `QualifiedNamespaces.cpp` o `QualifiedTypeNames.cpp` según el símbolo real, una cabecera de resolución compartida, pruebas de nombres y esta bitácora. Comprobar precedencia relativa, fallback global, ausencia y equivalencia contra el lookup existente; no alterar todavía el sembrado ni activar el driver.
- Evidencia 2026-11-05: `ProjectNameLookup::resolveNamespaceTarget` en `QualifiedTypeNames.cpp` usa `resolveQualifiedNamespaceTarget`, con la misma construcción y orden de candidatos y su propio predicado `namespaceExistsVisible`. La prueba dirigida confirmó búsqueda `A.B.C`, `A.C`, `C`, precedencia relativa, fallback global y ausencia. Reconstrucción ON y `git diff --check`: código 0; las demás pruebas ejecutadas de la suite no reportaron regresiones, pero la suite termina en código 1 por el sembrado global C++ previamente reproducido. No se ha demostrado equivalencia exhaustiva sobre todos los proyectos, ni se ha conectado todavía el productor de ámbitos.
- Validación adicional: `nsc` se reconstruyó en `A:/nsc-interop-build/tanda1-ON` y pasó la comprobación de runtime estático. Los proyectos mixtos C y C++ se compilaron y ejecutaron con resultados 85 y 148, respectivamente. Esto verifica sus rutas concretas de namespaces y llamadas; no demuestra toda la resolución del lenguaje ni corrige la prueba roja.
- Productor experimental de entornos léxicos: resolver las directivas de namespace usando `resolveQualifiedNamespaceTarget` y un inventario de namespaces visibles suministrado por el llamador; heredar imports de los padres sin contaminar hermanos y conservar alias para tratamiento posterior. Alcance: `include/nativesharp/Frontend/LexicalNamespaceEnvironment.h`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Probar prioridad relativa, deduplicación, alcance anidado, hermanos y targets ausentes. Es una etapa del productor, no conecta el driver ni considera resueltas las referencias a tipos o alias.
- Evidencia 2026-11-05: reconstrucción ON y `git diff --check` con código 0. La suite no reportó fallos en prioridad del namespace relativo `A.B.Inner.Shared` frente a `Shared`, imports duplicados, herencia padre-hijo, separación entre hermanos ni conservación local de un target ausente. Sigue terminando con código 1 por la interferencia ya conocida de la semilla global. El helper consume un inventario explícito de namespaces visibles, hereda y conserva alias sin resolverlos; no está conectado al parser. Siguiente acción: preparar ese inventario por proyecto y resolver alias/tipos antes de generar `ScopedKnownTypeNames`, incluyendo diferenciación de `using` a tipo y namespace y sin perder directivas fuera de la gramática léxica actual.
- Preparación de semillas experimentales: con los entornos por unidad y un inventario explícito de tipos C++, generar nombres completos y simples por ámbito mediante `qualifiedNameLadder`. Si hay alias o directivas sin resolver, la preparación debe rechazar el caso expresamente, no adivinar. Archivos: `include/nativesharp/Frontend/CppScopedTypeNames.h`, `tests/unit/CppClangSessionTests.cpp` y este registro. Comprobar parser real con `using` en un namespace, nombre calificado en otro y multiplicación sin `using`; todavía no conectar al driver ni considerar cubiertos alias, caché o visibilidad de proyecto.
- Primera comprobación: construcción ON exitosa; la prueba nueva de semillas acotadas con parser real no falló, y el rechazo de alias simples también pasó. La suite continúa con el fallo previo de inyección global. Se detectó al revisar el flujo que el inventario léxico omitía alias complejos como `using P = Shared.Item*`; eso impediría al productor rechazarlos. Corregir el inventario para conservar explícitamente directivas no representables y comprobar que el productor las rechaza. Archivos adicionales: `LexicalNamespaceScopes.h` y `LexicalNamespaceEnvironment.h`; no habilitar aún el productor en el driver.
- Evidencia 2026-11-05: reconstrucción ON y `git diff --check` con código 0. El productor `cppScopedTypeNames` usa `qualifiedNameLadder`, y la prueba con parser real confirmó un tipo simple en el ámbito con `using`, su nombre completo en el hermano y una multiplicación que permanece en el hermano. Los alias simples y el alias con puntero se conservan y se rechazan explícitamente por el productor experimental; no se presentan como soportados. La suite todavía termina con código 1 solo por la prueba del sembrado global vigente. El productor aún depende de inventarios suministrados manualmente y no está integrado en `ProjectCompilation`; su complejidad y la resolución de alias siguen pendientes. Siguiente acción: construir los inventarios por proyecto y resolver alias sin rechazar fuentes válidas, después conectar las unidades y la observación clonada sin reutilizar cachés incompatibles.
- Resolver experimentalmente alias simples a tipos C++ y namespaces, incluidas cadenas y sombreado de alias en ámbitos hijos. Conservar para cada alias su namespace de declaración; el índice de ámbitos generado debe consultar la tabla efectiva del ámbito más interno para que un alias hijo pueda ocultar el padre. Alcance: `LexicalNamespaceEnvironment.h`, `CppScopedTypeNames.h`, `ScopedKnownTypeNames.h`, `CppClangSessionTests.cpp` y esta bitácora. Seguir rechazando explícitamente sintaxis compleja no interpretada. No conectar aún al driver ni afirmar cobertura de alias nativos, precompilados o de todas las reglas de visibilidad.
- Evidencia 2026-11-05: reconstrucción ON y `git diff --check` con código 0. La prueba con parser real cubrió `using N = Shared`, `using T = Shared.Item`, `using U = T`, uso de `N.Item`, herencia de `U` por un namespace hijo y ausencia de `T` en el hermano. Otra prueba comprobó que un índice configurado con entornos efectivos consulta el ámbito más interno. Se guardan el namespace y los imports vigentes al declarar cada alias. La suite conserva código 1 exclusivamente por la prueba de inyección global anterior; no se ha probado la equivalencia completa del sombreado ni las colisiones entre alias nativos y C++. Los alias con modificadores y targets que no pertenecen al inventario C++ siguen rechazados, y el productor todavía no se conecta automáticamente. Próximo paso: obtener inventarios visibles por proyecto, ampliar alias y conectar parser y clonación con una política correcta de caché.
- Inventario por proyecto: obtener el cierre de `ProjectReference`, namespaces léxicos nativos y namespaces y nombres C++ provistos, sin filtrar información entre proyectos no visibles. Archivos: nueva cabecera de inventario en `include/nativesharp/Frontend`, `CppClangSessionTests.cpp` y esta bitácora. Comprobar propietario, dependencias transitivas y exclusión inversa; el consumidor sintáctico y los caches siguen sin cambiar hasta validar el inventario.
- Evidencia 2026-11-05: reconstrucción ON y `git diff --check` con código 0. Las pruebas no reportaron fallos en namespaces nativos y C++ con sus prefijos, cierre de tres proyectos, propiedad y exclusión de visibilidad inversa. El inventario aún es experimental y se le suministran sesiones/namespaces explícitos; no consume el catálogo completo de declaraciones nativas ni filtra accesibilidad de tipos individuales. La suite continúa en código 1 por la prueba de sembrado global. La caché sintáctica precompilada actual usa solo nombres C++ y no representa todas las entradas por proyecto y unidad: antes de cambiar la ruta de parseo se debe modificar su identidad o desactivarla en esta configuración y verificar además los productos publicados.
- Conexión experimental en `ProjectCompilation`: usar el inventario visible por proyecto y `cppScopedTypeNames` por unidad tanto en parseo inicial como en clonación, conservando el índice nativo previo. En esta configuración experimental, desactivar temporalmente la recuperación/publicación de caché sintáctica y productos publicados/snapshots para no reutilizar árboles sin identidad de ámbito comprobada. No activar desde el driver ni alterar la compilación que no proporciona tipos C++. Actualizar la prueba de caché para comprobar que no se crean ni reutilizan entradas con los tipos experimentales; verificar la regresión, clones, proyectos y tests existentes. Archivos: `ProjectCompilation.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Los rechazos explícitos para alias/targets aún no proyectables permanecen documentados y no son cierre de interoperabilidad.
- Primera ejecución: la interferencia se corrigió (`plain=1 imported=1`), pero quedaron dos fallos: el detector de puntero calificado buscaba solo `Token` y antes había contado erróneamente la declaración equivocada; además, agregar proyectos precompilados cambia los ordinales después de configurar el tipo experimental. Corregir el detector para reconocer `Shared.Token` y conservar la identidad del proyecto propietario por ruta antes de agregar precompilados, reasignando el ordinal al cerrar el grafo. Rechazar rutas o propietarios ambiguos en lugar de asociar el tipo a otro proyecto.
- Evidencia 2026-11-05: reconstrucción ON de `nsc` y `nsc_cpp_session_tests` exitosa; suite de sesiones código 0, incluida la comparación sin `using` (`plain=1 imported=1 qualified=1`) y la misma multiplicación en la sintaxis clonada. El caso con `using` conserva el comportamiento previo del parser nativo (`plain=1 imported=0 native=0`). La prueba de caché confirmó que, con tipos experimentales, no se publica ni reutiliza la entrada local de sintaxis sin esos tipos; la entrada nativa existente permanece intacta. Se corrigió el detector para exigir la variable `item` y la escritura calificada, y se reasignó el ordinal del propietario usando la ruta del proyecto al cerrar el grafo. Proyectos mixtos C y C++ ejecutados: 85 y 148; prueba de llamadas C/C++ y `git diff --check`: código 0; contrato de runtime estático de `nsc`: aprobado.
- Límites: este camino solo se activa mediante `setExperimentalCppImportedTypes`; el driver aún no descubre ni importa automáticamente clases C++. La identidad de caché por ámbito, recuperación de productos/snapshots, alias complejos, resolución de tipos nativos en alias, accesibilidad y rendimiento siguen pendientes. La prueba de productos publicados y la equivalencia exhaustiva de resolución de nombres no se han demostrado. Siguiente acción: conectar la obtención de records al grafo final y a la selección de funciones, con diagnósticos de conflictos y pruebas de extremo a extremo sin declaración sustituta, manteniendo temporalmente las salvaguardas de caché.
- Proveedor experimental de records: permitir que `ProjectCompilation` pida tipos a sus sesiones Clang después de cerrar el grafo y antes de materializar o cachear sintaxis; impedir que se configure simultáneamente con la lista explícita. El proveedor de prueba agrupa declaraciones, acepta solo definición única o declaración adelantada y rechaza conflictos/definiciones múltiples no verificadas, sin activarse en el driver. Probar el camino mixto existente desde el encabezado real sin declarar un sustituto ni configurar manualmente el tipo; las funciones continuarán configuradas explícitamente en esta etapa. Archivos: `ProjectCompilation.h/.cpp`, `CppSemanticRecord.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Cachés y productos permanecen desactivados cuando se obtengan tipos; ninguna prueba aislada se considera soporte de clases completo.
- Evidencia 2026-11-05: el proveedor obtiene `A.B.C` de una sesión Clang en el grafo final, instala su identidad y dimensiones y permite emitir el objeto NativeSharp que recibe una instancia C++ y pasa sus punteros a funciones C++. Se enlazó con la implementación C++ y el ejecutable de prueba pasó. Las funciones todavía se proyectaron y configuraron explícitamente; no se probó descubrimiento automático de funciones con records. La suite también comprobó rechazo de definiciones incompatibles, exclusión mutua con la lista manual, configuración inmutable tras comenzar y propiedad correcta al incorporar un módulo precompilado. Suite, prueba mixta, proyectos C y C++ (85 y 148) y `git diff --check`: código 0; `nsc` pasó su contrato de runtime estático. El proveedor no está activado en el driver, rechaza grupos en conflicto o con múltiples definiciones no verificadas y omite records no proyectables; falta resolución por demanda, manejo de colisiones NativeSharp y cobertura de vida de objetos y miembros.
- Proyección experimental por demanda: extender el proveedor de funciones para intentar firmas con punteros a records únicamente cuando el lookup de la compilación identifique esos records como C++ importados de su propietario real. Conservar la ruta escalar existente y las declaraciones no proyectables como candidatas diferidas; no activar esta extensión en el driver. Sustituir en la prueba mixta la lista manual de funciones por el proveedor por demanda y obtener namespaces de la sesión Clang. Archivos: `CppSemanticFunctionGroups.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Verificar emisión y ejecución de la llamada y comprobar que un tipo NativeSharp homónimo no se acepte como record C++.
- Evidencia 2026-11-05: la prueba mixta obtuvo de la sesión tanto el record como las funciones `create` y `consume`, seleccionadas por demanda, sin lista manual de tipos o funciones. El objeto NativeSharp se verificó con LLVM, se enlazó con la implementación C++ y se ejecutó correctamente. La prueba de un `struct A.B.C` nativo homónimo comprobó que `consume(C*)` queda diferida y no se proyecta como si el tipo nativo fuera el record C++. Suite de sesiones, ejecución mixta, proyectos C y C++ (85 y 148) y `git diff --check`: código 0; `nsc` pasó su contrato de runtime estático. La ruta por demanda y el proveedor de records siguen siendo experimentales y no están conectados al driver; permanecen pendientes selección/diagnóstico seguro de colisiones, encabezados reales, miembros y vida de objetos, además de alias y caché.
- Alias con sintaxis de tipo explícita: la clasificación previa rechazaba todo alias con modificadores o tipos integrados, incluso si el parser solo necesita saber que el alias nombra un tipo y el resolvedor semántico vigente conserva su sintaxis completa. Registrar esa forma en el inventario léxico y sembrar su nombre como tipo sin sustituir ni validar el target; dejar que el parser y el resolvedor de NativeSharp hagan esas comprobaciones. Alcance: `LexicalNamespaceScopes.h`, `CppScopedTypeNames.h`, `CppClangSessionTests.cpp` y esta bitácora. Comprobar alias a puntero de record, alias a tipo integrado y errores sintácticos reales; no activar todavía el proveedor en el driver ni afirmar soporte de alias genéricos o colisiones sin pruebas.
- Evidencia 2026-11-05: el inventario identifica como sintaxis de tipo los alias a tipos integrados y targets con modificadores, preservando la validación y expansión de la sintaxis completa en el parser y lookup existentes. La prueba con `using P = Shared.Item*` y `using Count = int` pasó; un alias con sintaxis inválida siguió siendo rechazado y produjo diagnóstico del parser. La prueba mixta por demanda cambió a `using Ptr = A.B.C*; Ptr p = A.B.create();`, emitió el objeto y se enlazó y ejecutó correctamente. Suite de sesiones, llamadas mixtas, proyectos C y C++ (85 y 148), `git diff --check` y contrato de runtime estático: código 0. No se ha probado la totalidad de alias genéricos, colisiones ni equivalencia del lector léxico con todas las formas del lenguaje; el driver aún no habilita los records.

## Selección experimental de records después de observar nombres

- Entrada y causa: `provideExperimentalCppRecordTypes` rechaza todos los grupos con definiciones conflictivas o aún no verificadas antes de analizar los usos; el lookup rechaza una identidad NativeSharp instalada simultáneamente como C++. Ambos comportamientos están demostrados por las pruebas existentes, incluso cuando el tipo C++ no se usa.
- Objetivo de este paso: calcular una selección aislada para el lookup a partir del resultado de `observeCppRecords`, sin alterar todavía el proveedor ni conectarla al driver. Conservar los candidatos válidos no colisionados, excluir colisiones nativas y grupos no válidos sin usos, y reportar un grupo no válido observado como referencia o candidato de expresión. La selección de expresiones es conservadora y no prueba que el candidato se haya utilizado realmente.
- Archivos autorizados: `include/nativesharp/Interop/CppSemanticRecord.h`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `tests/unit/CppClangSessionTests.cpp` y esta bitácora. Quedan protegidas las demás rutas; sin despliegue ni cambios de herramientas. Validar identidades, propietarios, índices y ausencia de duplicados antes de publicar salida; probar selección, colisión, conflicto usado/no usado y entradas inconsistentes. No afirmar soporte productivo ni resolución de todos los usos implícitos.
- Evidencia 2026-11-05: se añadió `selectExperimentalCppRecordTypes`, que reconstruye candidatos desde inventario y grupos, valida índices, duplicados y coherencia de colisiones, conserva tipos válidos no colisionados, difiere grupos en conflicto no observados y rechaza los observados como tipo o candidato de expresión. La suite contrasta entradas armadas con inventario real de Clang y observaciones reales de cinco proyectos con y sin colisión NativeSharp; verifica conflicto usado/no usado e índices y colisiones incoherentes. Reconstrucción de `nsc_cpp_session_tests` y ejecución con Clang de `A:/al82dev-clang`: código 0; `git diff --check`: código 0 para archivos rastreados. El selector no está conectado al flujo: el proveedor temprano sigue rechazando grupos problemáticos globalmente. Falta integrar la selección sin alterar las semillas del parser y demostrar dependencias implícitas de firmas y errores con headers reales.

## Integración aislada de la selección de records

- Cambio funcional experimental: suministrar al parser los candidatos preparados sin rechazar de antemano conflictos y permitir un selector opcional antes de construir el lookup definitivo. La lista temprana permanece intacta para volver a analizar copias y mantener la política de caché; la lista seleccionada vive únicamente durante la construcción de nombres. El proveedor estricto anterior permanece disponible y el driver no habilita esta ruta.
- Archivos y símbolos: `ProjectCompilation.h/.cpp` para el callback opcional y su invocación; `CppSemanticRecord.h/.cpp` para el proveedor de candidatos y adaptador de observación/selección; `CppClangSessionTests.cpp` para comprobar llamadas con punteros, colisiones nativas, conflictos usados y no usados y configuración tardía; esta bitácora. Demás fuentes protegidas. Sin despliegue ni cambios en las otras rutas.
- Preservar el recorrido normal y las pruebas existentes; validar que un conflicto usado no pueda entrar al lookup, que uno no usado no lo detenga y que el tipo nativo siga siendo nativo. La selección de candidatos de expresión es conservadora; los usos implícitos de firmas y la equivalencia total de sintaxis todavía necesitan evidencia antes de habilitarse en el driver.
- Evidencia 2026-11-05: `ProjectCompilation` acepta el selector experimental antes de iniciar y aplica su resultado localmente al lookup, conservando sin modificar las semillas usadas para el parser y las copias. `provideExperimentalCppRecordCandidates` prepara esas semillas sin el rechazo global del proveedor estricto; `selectExperimentalCppRecordTypesForCompilation` observa y filtra antes de instalar tipos. Con dos sesiones Clang de definiciones incompatibles, la compilación de nombres pasa si el record no se usa, rechaza una referencia C++ y preserva un struct NativeSharp homónimo. La prueba de punteros obtiene `A.B.C`, `create` y `consume` de un encabezado físico `record-api.h`, genera objeto LLVM y enlaza y ejecuta contra la implementación C++. La suite de sesiones y `pruebas-llamadas-escalares/ejecutar.cmd` pasaron tras cambiar el fixture a archivo; los proyectos C y C++ dieron 85 y 148, y `git diff --check` pasó para archivos rastreados. `nsc.exe` pasó el contrato de runtime estático. Esta integración permanece fuera del driver: conflictos procedentes solo de firmas, candidatos de expresión con sombras, genéricos, cachés y vida de objetos requieren más trabajo.

## Verificación de records usados solo por firmas

- Pregunta: una llamada `A.B.consume(null)` no menciona `A.B.C` en sintaxis NativeSharp, pero la firma C++ requiere `C*`. El proveedor experimental filtra los grupos no instalados en el lookup y deja diferidas las firmas que no puede proyectar. Hay que verificar el resultado real: un grupo válido debe permitir llegar a MIR y uno conflictivo no debe generar una llamada aceptada.
- Presupuesto de esta comprobación: `tests/unit/CppClangSessionTests.cpp` y esta bitácora; usar las sesiones Clang y el selector experimental existentes. No cambiar el driver ni la semántica compartida. Si aparece un defecto, registrar la causa y el presupuesto específico antes de corregirlo.
- Evidencia 2026-11-05: dos compilaciones usan exactamente `A.B.consume(null)` con `consume(C*)` sin nombrar `C` en NativeSharp. Con una definición válida, la ruta experimental proyecta la función y llega a MIR. Con dos definiciones C++ incompatibles, no llega a MIR, produce diagnóstico y conserva `A::B::consume` como candidato diferido de llamada directa con una razón registrada. La reconstrucción y ejecución de `nsc_cpp_session_tests` terminaron con código 0. Esta prueba cubre el parámetro implícito y la ruta de rechazo, no demuestra inferencia de retornos, selección entre sobrecargas complejas ni un diagnóstico que necesariamente explique el conflicto de definición.
- Siguiente comprobación: ampliar el mismo fixture con `var p = A.B.create(); A.B.consume(p)`, donde el tipo del puntero proviene del retorno C++. Verificar definición válida y conflictiva, conservar los casos anteriores y exigir causa observable para el rechazo. Presupuesto sin cambios: `CppClangSessionTests.cpp` y esta bitácora.
- Evidencia 2026-11-05: la suite amplía los dos escenarios al retorno `C*` inferido mediante `var` y lo entrega a `consume`. Con definición única llega a MIR; con definiciones incompatibles falla, deja `create` o `consume` como llamada diferida con razón y emite diagnóstico. La reconstrucción y ejecución de `nsc_cpp_session_tests` pasaron. El caso de inferencia se verificó hasta MIR, no se enlazó ni ejecutó de forma específica; permanece experimental y no demuestra otras clases de inferencia o de overloads.

## Ejecución del retorno C++ inferido

- Objetivo: verificar desde Clang y el selector experimental hasta ejecución la llamada `var p = A.B.create(); return A.B.consume(p + 1);`, sin declaración explícita de `A.B.C` en NativeSharp. El tipo y las dimensiones del fixture C++ deben coincidir exactamente entre el encabezado analizado y la implementación enlazada.
- Archivos: `tests/unit/CppClangSessionTests.cpp`, `pruebas-llamadas-escalares/ejecutar.cmd`, nuevo `pruebas-llamadas-escalares/cpp-record-inferred-externo.cpp` y esta bitácora. La prueba emitirá objeto independiente bajo `A:/nsc-interop-build`, verificará el IR, enlazará contra el fixture C++ y comprobará el valor devuelto. No habilitar el driver ni cambiar el runtime o fuentes ajenas.
- Evidencia 2026-11-05: la sesión experimental analizó el encabezado físico `record-api.h`; NativeSharp infirió `C*` desde `create()`, calculó `p + 1` y llamó a `consume`. El IR superó la verificación LLVM, se emitió `cpp-record-inferred-call.obj` y se enlazó con una implementación C++ que confirma que el puntero corresponde al segundo elemento y devuelve 23. `cpp-record-inferred-call.exe`, la suite de sesiones y el conjunto `ejecutar.cmd` finalizaron con código 0. Esto verifica el recorrido experimental de punteros e inferencia para ese contrato, no construcción, destrucción, campos o soporte general de clases.

## Comparación estructural experimental entre sesiones Clang

- Necesidad: el mismo encabezado puede producir varias definiciones del mismo record en sesiones diferentes; actualmente todas se clasifican `MultipleDefinitionsUnverified`. `ODRHash`, dimensiones y ruta por sí solos no prueban equivalencia. Clang 23.1.0 ofrece `StructuralEquivalenceContext` para contrastar dos AST, pero esa comparación tampoco demuestra por sí sola ODR ni compatibilidad ABI completas.
- Presupuesto: `CppInteropSession.h`, `ClangSession.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Agregar una consulta experimental que compare records completos y no ambiguos por nombre, con locks de ambas sesiones, mismo target e idioma y comparación de tamaño y alineación; resultado desconocido ante sesiones incompatibles, cerradas, tipos ausentes o ambiguos. No modificar la agrupación ni aceptar definiciones múltiples automáticamente. Probar coincidencias, diferencias estructurales con igual tamaño, tipos ausentes, sesión cerrada, misma sesión y condiciones incompatibles.
- Evidencia 2026-11-05: `structurallyEquivalentRecord` usa `clang::StructuralEquivalenceContext` en dos AST protegidos por sus locks, con tratamiento específico de la misma sesión; comprueba target, idioma, estándar, dimensiones e identidad de nombre no ambigua. La prueba abre dos veces un encabezado físico y obtiene equivalencia; con otra sesión del mismo archivo y un macro que cambia un miembro de `int` a `float`, ambos de igual tamaño, obtiene diferencia. También comprueba sesión idéntica, record ausente, estándar diferente y sesión cerrada. Reconstrucción de NSC y suite de sesiones: código 0; `ejecutar.cmd`: código 0; contrato de runtime estático y `git diff --check`: correctos. La rama de ambigüedad y la concurrencia inversa todavía no tienen prueba específica. Esta consulta no modifica la agrupación, no demuestra ODR ni ABI completos y permanece sin activar en el driver.

## Evidencia estructural por grupo de records

- Objetivo: relacionar los grupos con múltiples definiciones del inventario con sus sesiones propietarias y comparar cada definición completa con la primera. Registrar por grupo `coincidente`, `diferente` o `no disponible`, sin cambiar el estado del grupo ni tratar la comparación como aceptación ODR/ABI. Validar propietarios, índices, identidades y sesiones antes de comparar; una diferencia prevalece sobre una comparación no disponible.
- Archivos: `CppSemanticRecord.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Pruebas: inventario real con definiciones iguales y diferentes, varias sesiones, entradas inválidas y sesión cerrada; conservar todos los tests anteriores. La conexión al selector o al driver requiere un paso y evidencia posteriores.
- Evidencia 2026-11-05: `compareCppRecordDefinitions` devuelve evidencia separada por grupo, conserva el número de definiciones, comprueba identidad y procedencia de los registros, rechaza índices o target inconsistentes y compara sesiones con Clang. En un inventario real identificó dos definiciones `Equal` estructuralmente coincidentes y tres definiciones `Same` y `Shared` diferentes; con sesiones cerradas informó `Unavailable` y mantuvo `Different` cuando ya se había detectado una diferencia. Una primera ejecución mostró que el fixture cerraba sesiones antes de una prueba anterior; se corrigió el orden y la suite completa volvió a pasar. La reconstrucción de NSC y de `nsc_cpp_session_tests`, la ejecución de la suite, el contrato de runtime estático y `git diff --check` terminaron correctamente. No cambia los estados originales ni permite todavía consumir el grupo: no es una demostración completa de ODR ni ABI.

## Transporte experimental de punteros a definiciones repetidas

- Objetivo: dejar que la ruta experimental transporte punteros de un record con múltiples definiciones únicamente si Clang compara sus AST como estructuralmente equivalentes y todas las definiciones completas conservan huellas, tamaños y alineaciones coincidentes. La condición aporta evidencia para este uso acotado, no certifica ODR o ABI completa. La representación sigue sin layout: no habilita aritmética de punteros, `sizeof`, campos ni objetos por valor.
- Presupuesto: `CppSemanticRecord.h/.cpp` para el estado y refinamiento aislado y su aplicación al proveedor y observador experimentales; `CppSemanticFunction.cpp` y `CppSemanticFunctionGroups.cpp` para proyectar exclusivamente punteros con el estado refinado; `CppClangSessionTests.cpp`, `pruebas-llamadas-escalares/ejecutar.cmd`, un fixture C++ de ejecución y esta bitácora. El proveedor estricto, la clasificación inicial y el driver no cambian.
- Verificar dos sesiones del mismo encabezado: llamada y retorno por puntero enlazados y ejecutados, rechazo de aritmética por ausencia de layout, diferencias, evidencia ausente y sesiones cerradas. Preservar casos de conflicto, colisiones y comportamiento sin interoperabilidad.
- Evidencia 2026-11-05: `refineCppRecordGroupsForPointerTransport` marca el caso `MultipleDefinitionsPointerCompatible` solo tras comparación estructural positiva y huellas, tamaños y alineaciones completos e iguales; mantiene conflictos, faltantes y sesiones cerradas sin promover. El observador y el proveedor experimental de funciones reconocen ese estado únicamente para proyección de punteros, sin publicar dimensiones. Dos sesiones del mismo encabezado físico permitieron inferir y pasar el puntero C++; el objeto LLVM verificado se enlazó y ejecutó con el fixture C++, que comprobó la dirección y devolvió 19. La aritmética sobre ese puntero se rechazó por falta de layout. `nsc_cpp_session_tests`, el conjunto ejecutable de llamadas, el contrato estático de NSC y `git diff --check` pasaron. El driver sigue sin habilitar records; la comparación no certifica ODR/ABI completa, miembros ni vida de objetos.
- Regresión adicional: proyectos de prueba C y C++ ejecutados con el `nsc` reconstruido de A: produjeron 85 y 148, respectivamente.

## Auditoría de reserva cruda para records C++

- Riesgo a verificar: `valueLayoutForTypeKey` dispone de las dimensiones de un record C++ importado, mientras `validateRawAllocationElement` consulta la trivialidad a través del modelo NativeSharp; no se ha demostrado que un record C++ con constructor y destructor no triviales sea rechazado en `stackalloc` y `new` crudo. El tamaño no demuestra las reglas de vida de objetos.
- Presupuesto de reproducción: `CppClangSessionTests.cpp` y esta bitácora. Crear sesiones Clang con un record cuyo constructor y destructor están declarados, compilar ambas formas hasta MIR y comprobar diagnóstico explícito relacionado con la vida del objeto. No alterar código del compilador hasta demostrar la ruta exacta.
- Reproducción 2026-11-05: con `namespace A::B { struct C { C(); ~C(); int value; }; }` obtenido de Clang, ambas expresiones `stackalloc A.B.C[1]` y `new A.B.C` llegaron a MIR sin diagnóstico. `validateRawAllocationElement` consulta `elementTypeHasTrivialLifetime` sobre un placeholder NativeSharp sin los miembros C++ y lo trata como trivial; el layout real permite avanzar. Es un comportamiento incorrecto en la ruta experimental, no evidencia de soporte de construcción C++.
- Presupuesto correctivo: únicamente `ProjectMethodBodyModel::validateRawAllocationElement` en `src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp`, la prueba anterior y esta bitácora. Antes de consultar trivialidad, detectar la identidad importada C++ del elemento directo y rechazar las dos reservas crudas con diagnóstico de vida hasta implementar el contrato; no modificar la política de reservas de tipos nativos ni de punteros. Verificar ambas reproducciones y conservar las pruebas existentes de interoperabilidad.
- Evidencia 2026-11-05: la validación identifica el elemento directo por el índice de tipos importados y rechaza `stackalloc A.B.C[1]` y `new A.B.C` con un diagnóstico que indica que falta implementar la vida del objeto C++. La misma suite comprobó que siguen llegando a MIR `stackalloc A.B.C*[1]` (almacenamiento para punteros) y `stackalloc int[1]`. Reconstrucción de NSC y suite, ejecución de llamadas mixtas, proyectos C y C++ (85 y 148), contrato estático y `git diff --check`: código 0. Esto evita esas dos reservas incorrectas en la ruta experimental; no implementa constructores ni certifica todos los caminos posibles de reserva o almacenamiento.

## Identidad experimental de métodos C++ de instancia

- Objetivo: preparar el puente entre una declaración de método obtenida por Clang y el futuro modelo de miembros de NSC, conservando identidad del tipo, símbolo físico, receptor `const`, firma y tipos escalares. Es una representación aislada: no instalarla aún como miembro ni afirmar que ya se emiten llamadas o se gestionan excepciones.
- Presupuesto: `CppSemanticFunction.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Proyectar métodos ordinarios públicos de instancia, no virtuales y de convención `cdecl` en el target Windows x64; diferir explícitamente otros contratos, sin descartar el inventario original. Comprobar miembros extraídos de Clang y errores por receptor o tipos incompatibles. El resto de características continúa dentro del objetivo global.
- Ampliación necesaria del mismo contrato: `CppRecordDeclaration.h` y `ClangSession.cpp` para conservar `volatile` del receptor, que la instantánea actual omite. Sin ese dato no se puede distinguir un método ordinario de uno con receptor volátil; la proyección aislada conserva la diferencia y no la implementa silenciosamente.
- Evidencia 2026-11-05: la instantánea Clang conserva `isVolatile` y `projectCppSemanticInstanceMethod` crea una representación independiente con propietario, símbolo mangled, firma canónica, receptor `const` y tipos escalares. Con declaraciones reales de Clang la prueba conserva `MyClass::A(int) const` y difiere métodos privados, estáticos, virtuales, con calificador de referencia o receptor volátil; también rechaza índices o target inválidos. La reconstrucción de NSC y la suite, el contrato de runtime estático, el conjunto de llamadas mixtas y `git diff --check` pasaron. No está instalada en `ProjectMemberModel`, no emite el receptor ni una llamada, y no define el cruce de excepciones: la interoperabilidad de métodos de instancia sigue pendiente.

## Índice experimental de métodos de instancia

- Objetivo: convertir explícitamente la información proyectada de un método Clang en una declaración independiente del frontend y registrarla en un `ProjectMemberModel` aislado antes de construir sus índices. Exigir identidad de tipo C++ ya instalado, propietario correcto, firma escalar, símbolo físico y ausencia de duplicados; conservar `const` del receptor. No usar metadatos NSMETA para representar el origen C++ ni habilitar el driver o el HIR.
- Presupuesto: nuevo `include/nativesharp/Frontend/CppImportedInstanceMethod.h`; `ProjectMemberModel.h/.cpp` para la configuración e indexación; `ClassMemberSymbol.h` para marcar el origen; `CppSemanticFunction.h/.cpp` para convertir la proyección; `CppClangSessionTests.cpp` y esta bitácora. Verificar búsqueda por tipo/nombre, identidad física, destinatario incorrecto, duplicados y configuración tardía. No afirmar todavía resolución de llamadas ni ABI.
- Evidencia 2026-11-05: una proyección real de `MyClass::A(int) const` se convirtió a `CppImportedInstanceMethod` y se indexó en un `ProjectMemberModel` aislado sobre el tipo C++ ya registrado. La búsqueda por propietario y nombre conservó símbolo físico, receptor implícito, marca `const` y tipos; el modelo rechazó duplicados, propietario incorrecto y cambios tras construirse. Una primera reconstrucción detectó una sobrecarga ambigua de `DiagnosticEngine::report` por un rango vacío; se hizo explícito `SourceRange` y la reconstrucción posterior y la suite pasaron. El conjunto de llamadas mixtas, los proyectos C y C++ (85 y 148), el contrato estático y `git diff --check` también pasaron. El índice sigue desconectado de `ProjectCompilation` y del driver; `isCppExternal` y su símbolo no se han incorporado a snapshots ni a HIR/MIR, por lo que no se ha probado ni habilitado una llamada de método C++.

## Clasificación de excepciones en métodos C++

- Objetivo inmediato: conservar en la declaración experimental de cada método si Clang considera que su especificación de excepciones impide que estas se propaguen. Una especificación aún no resuelta se trata conservadoramente como desconocida; no habilitar llamadas ni activar el proveedor en el driver.
- Presupuesto: `CppRecordDeclaration.h`, `ClangSession.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Verificar declaraciones reales con `noexcept`, `noexcept(false)` y sin especificación; reconstruir y ejecutar la suite. Esta clasificación no implementa interoperabilidad de excepciones ni demuestra por sí sola la seguridad de una llamada.
- Evidencia: la extracción utiliza `FunctionProtoType::isNothrow()` solo si la especificación está resuelta, y conserva `false` ante las demás situaciones. La suite con declaraciones C++ reales distinguió `noexcept`, `noexcept(false)` y ausencia de especificación; reconstrucción de `nsc` y `nsc_cpp_session_tests` y ejecución de esta última finalizaron con código 0. Todavía no se usa este dato para permitir llamadas, no se ha implementado propagación de excepciones ni se ha alterado la protección previa a HIR.
- Siguiente cambio experimental: limitar temporalmente la proyección de métodos de instancia a especificaciones no lanzadoras verificadas por Clang. Es una protección mientras se diseña la propagación de excepciones; los demás métodos permanecen en el inventario y siguen dentro del objetivo general. Actualizar los encabezados de prueba y verificar tanto el caso permitido como los diferidos.
- Resultado: la proyección experimental conserva el método `noexcept` y difiere tanto `noexcept(false)` como un método sin especificación; la prueba de encabezado físico usa ahora `ping(int) const noexcept`. La reconstrucción y la suite terminaron con código 0. Sigue sin emitirse ninguna llamada de instancia y el driver no activa el proveedor.

## Exploración aislada de llamadas de instancia

- Objetivo: investigar si el resolvedor existente acepta un receptor de record C++ y comprobar cómo transportar el símbolo y el receptor por HIR. La protección normal anterior a HIR permanece activa; se habilita solo mediante una opción explícita para pruebas, no conectada al driver. No habilitar construcción de objetos ni prometer ABI general.
- Presupuesto: `ProjectCompilation.h/.cpp`, `HirBuildCoordinator.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Comprobar selección y declaración externa con un método escalar `noexcept` de un encabezado real, y verificar que sin la opción se mantiene el rechazo. Registrar cualquier falla sin presentarla como interoperabilidad completa.
- Evidencia inicial: con la opción exclusiva de prueba activada, `(*p).ping(7)` llegó a MIR y produjo un módulo LLVM verificado que contiene `?ping@Widget@Demo@@QEBAHH@Z`, obtenido del encabezado Clang. El caso usa un puntero nulo y **no se ejecutó**; por lo tanto aún no demuestra el paso correcto de `this` ni el ABI en ejecución. La protección sin la opción sigue rechazando el avance; el driver no habilita la opción. Reconstrucción y suite finalizadas con código 0. La representación HIR reutiliza provisionalmente la forma de declaración externa ya existente, con el símbolo Clang; no se ha afirmado compatibilidad con NSMETA ni cachés de métodos C++.
- Siguiente acción: comprobar que el LLVM generado contiene tanto la declaración como la llamada con el receptor y ejecutar un caso enlazado con un objeto C++ real antes de considerar cualquier activación fuera de pruebas.
- Ampliación del experimento: incluir en el presupuesto el fixture C++ y `ejecutar.cmd` de `pruebas-llamadas-escalares`. Usar una fábrica C++ que devuelva un objeto real y comprobar que el método recibe ese mismo objeto y el argumento correcto; seguir usando la opción aislada y sin activar el driver.
- Evidencia de ejecución: `Demo.create()` devuelve una instancia C++ real y NativeSharp ejecuta `(*p).ping(7)`. El método comprueba la identidad de `this` y el argumento, devuelve 31 y el proceso finaliza con código 0. El módulo LLVM verificado contiene tanto la declaración como la llamada al símbolo exacto de Clang. El primer enlace falló por la referencia a `__nativesharp_trap` que genera el acceso por puntero; el fixture aislado añadió una implementación de prueba con la firma `void(i32)` del contrato de runtime, y el enlace y conjunto de llamadas mixtas pasaron. La suite y la reconstrucción también pasaron. Esto cubre una firma escalar `const noexcept` para Windows x64, no constructores, métodos virtuales, propiedad, excepciones ni soporte general; sigue protegido y desactivado en el driver.
- Ampliación: ejercitar además un método no `const` que modifique un campo de la instancia C++ y otro `const` que observe ese cambio, sin exponer aún el acceso directo a campos en NativeSharp. Repetir la ejecución del fixture y la suite.
- Resultado: NativeSharp invoca `mutate(7)` y luego `ping(5)` sobre el mismo objeto creado en C++; el primero modifica un campo y el segundo comprueba el valor actualizado. El ejecutable comprueba el resultado combinado 42 y el conjunto de llamadas mixtas terminó con código 0; la reconstrucción y la suite pasaron. La restricción de uso experimental y los límites anteriores siguen vigentes.
- Comprobación adicional: usar la sintaxis de puntero `p->metodo()` en el mismo caso ejecutable, que el parser de NSC transforma a acceso sobre el puntero desreferenciado. No inferir soporte para referencias, punteros inteligentes ni métodos virtuales a partir de esta prueba.
- Resultado: `p->mutate(7)` y `p->ping(5)` compilaron y el proceso mixto comprobó el resultado 42 con código 0. La suite volvió a pasar. Es el mismo contrato escalar experimental, ahora con la sintaxis directa de puntero.
- Nueva comprobación de ABI: añadir un método `const noexcept` con argumento y retorno `double`, para verificar en ejecución la combinación del receptor implícito con los registros de punto flotante de Windows x64. No extrapolar a estructuras por valor, varargs ni otras plataformas.
- Resultado: `p->factor(1.5)` recibió el receptor y argumento correctos y devolvió 13.0; junto con los métodos enteros, el proceso validó 55. El primer enlace solicitó `_fltused` porque el arnés prescinde del CRT; el fixture proporcionó ese marcador y el conjunto de llamadas terminó con código 0. La reconstrucción y suite también pasaron. Esto no demuestra la distribución ni el enlace de una aplicación completa con sus runtimes.
- Nueva comprobación: combinar argumentos enteros y `double` en un mismo método de instancia, incluyendo un argumento que exceda los primeros registros, para comprobar orden y paso por pila en Windows x64. Mantener el mismo aislamiento y las demás restricciones.
- Resultado: `mixed(3, 2.5, 9, 4.5)` verificó el receptor y los cuatro argumentos; el proceso comprobó el resultado combinado 72 y terminó con código 0. La suite y su reconstrucción pasaron. La prueba cubre esa firma concreta, incluido su último argumento en pila según el ABI Windows x64.
- Regresión adicional: los proyectos C y C++ con el `nsc` construido en A: volvieron a ejecutar con resultados 85 y 148 respectivamente; `git diff --check` en NSC pasó. No se ejecutó `ctest`, que en esta configuración apunta al compilador de C:.
- Investigación siguiente: repetir el fixture con `class` C++ en vez de `struct`, conservando el acceso público y la disposición equivalente, y verificar si el receptor NativeSharp mantiene la dirección correcta. No extrapolar de una forma a otra sin ejecución.
- Resultado inicial: con `class Widget` y miembros públicos, la suite y el proceso mixto con resultado 72 pasaron. Para conservar una regresión reproducible de ambas formas, añadir un segundo tipo `struct` al mismo fixture y ejecutar un método de cada uno. Esto no verifica diferencias de acceso privado ni construcción propia desde NativeSharp.
- Resultado conservado: el mismo ejecutable llama a métodos de `class Widget` y `struct Thing`, ambas instancias creadas en C++; los métodos comprobaron sus receptores y el proceso validó 91. Reconstrucción, suite y conjunto mixto pasaron. Las limitaciones anteriores siguen vigentes.
- Punto por investigar antes de cerrar excepciones: la descripción actual `CppFunctionDeclaration` de funciones libres no conserva su especificación de excepciones, a diferencia del nuevo dato experimental de métodos. Las pruebas de funciones libres anteriores no demuestran propagación ni limpieza ante excepciones C++; no asumirla. Hay que seguir su emisión y probar el contrato antes de decidir cómo aceptar o diagnosticar ese caso, sin suponer que todo `extern "C"` garantiza ausencia de excepciones.
- Acción: conservar la clasificación conservadora de Clang también para funciones libres, sin cambiar todavía qué funciones acepta el driver. Presupuesto: `CppFunctionDeclaration.h`, `ClangSession.cpp`, `CppClangSessionTests.cpp` y bitácora. Comprobar `noexcept`, ausencia de especificación y enlace C declarado en C++; esto es información, no una implementación de captura o propagación.
- Resultado: la instantánea distinguió `cpp_noexcept` de una función sin especificación y de `extern "C"` sin `noexcept`; este último no se asumió como no lanzador. Reconstrucción de NSC, contrato estático del host, suite de sesiones y conjunto de llamadas mixtas pasaron. El comportamiento del driver no cambió y la compatibilidad de excepciones sigue pendiente.
- Cobertura adicional: una declaración `extern "C" ... noexcept` se clasificó como no lanzadora y una declaración C++ `noexcept(false)` como potencialmente lanzadora; la suite pasó.

## Proyección aislada de campos de instancia

- Objetivo: preparar una descripción de campos escalares públicos obtenidos de Clang, conservando propietario, tipo, `const`, `volatile` y desplazamiento en bytes. Validar la disponibilidad del layout, que no sea bitfield, que el campo quepa en el objeto y que el desplazamiento esté alineado a byte. Se empleará alineación conservadora en una futura lectura; esta proyección no habilita acceso ni prueba vida del objeto.
- Presupuesto: `CppSemanticRecord.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Comprobar campos públicos y privados, bitfields, layouts ausentes, límites y calificadores con datos reales de Clang y casos negativos aislados. Los campos complejos siguen dentro del objetivo global.
- Evidencia: desde un record C++ real se proyectó el campo público `count` con desplazamiento de 12 bytes y propietario correcto; se rechazaron el campo privado, un layout fuera de límites y un target distinto. Otro record conservó `const` y `volatile` y rechazó un bitfield. Reconstrucción de NSC y suite con Clang terminaron con código 0. Es solo metadata aislada, todavía no se indexan ni leen campos desde NativeSharp; faltan pruebas de layout ausente y otros casos negativos explícitos.
- Comprobación adicional: la suite pasó al ensayar tamaño ausente, desplazamiento ausente, desplazamiento que no cae en byte y unión C++; los cuatro quedan diferidos. No se han implementado las operaciones sobre ellos.
- Siguiente paso aislado: convertir esos metadatos en una entrada de campo del `ProjectMemberModel` sin conectarla a `ProjectCompilation` ni al driver. Conservar el desplazamiento de Clang y los calificadores; verificar búsqueda, duplicados y propietario. El uso de `readonly` para transportar `const` aún no demuestra la semántica completa de C++.
- Archivos de ese paso: nuevo `CppImportedInstanceField.h`, `ProjectMemberModel.h/.cpp`, `CppSemanticRecord.h/.cpp` y `CppClangSessionTests.cpp`; la entrada solo se acepta si el propietario importado tiene layout suficiente para alojarla.
- Evidencia: el índice aislado encontró `count` junto al método `A`, conservando tipo y desplazamiento 12; rechazó duplicados, propietario incorrecto, campo fuera del objeto y colisión con el nombre de un método. Una prueba sintética confirmó que conserva las marcas `readonly` y `volatile`; la semántica de acceso aún no está verificada. Reconstrucción de NSC, contrato estático del host y suite pasaron. Sigue sin existir proveedor de campos en `ProjectCompilation` ni uso de estos campos en HIR/MIR.
- Tras incorporar el índice, el conjunto de llamadas mixtas volvió a pasar y `git diff --check` no reportó errores. El modelo de layout actualmente omite los structs importados al construir sus layouts ordinarios: antes de emitir accesos hay que incorporar desplazamientos de Clang explícitamente sin crear una representación NativeSharp falsa ni habilitar reservas o destrucción.
- Siguiente paso: conectar un proveedor experimental de campos de definiciones únicas a `ProjectCompilation`, conservando la protección que bloquea su avance antes de HIR aunque esté habilitada la prueba de métodos. Desactivar snapshots/cachés que no conocen los campos. Presupuesto: `ProjectCompilation.h/.cpp`, `ProjectCompilationSnapshots.cpp`, `CppSemanticRecord.h/.cpp`, `CppClangSessionTests.cpp` y bitácora; driver fuera de alcance de este paso.
- Evidencia: una sesión de Clang sobre encabezado físico alimentó automáticamente el miembro `Demo.Widget.value` con tipo y desplazamiento 0. Otra compilación independiente confirmó que los campos bloquean la generación incluso con la opción de pruebas de métodos activada. La primera versión de la prueba reutilizó una instancia de `ProjectCompilation` y falló porque esas instancias son de un solo uso; se corrigió empleando una nueva instancia. Reconstrucción de NSC, contrato estático, suite y llamadas mixtas pasaron. Snapshots y cachés relevantes se desactivaron para esta ruta, pero la combinación con módulos precompilados todavía necesita pruebas específicas. El driver no activa el proveedor.
- La construcción de `nsc_frontend` con la integración Clang desactivada también terminó con código 0; no se ejecutó `ctest` porque apunta a C: en esa configuración.
- Hallazgo para el acceso futuro: el LLVM de lectura y escritura ordinarias de campos no aplica actualmente la alineación del nodo en todas sus rutas; usar un campo C++ empaquetado podría crear una suposición de alineación incorrecta. Mientras se corrige y prueba el lowering, la proyección aislada diferirá campos cuyo offset o alineación del objeto no aseguren la alineación natural del tipo. No se considera soporte de layouts empaquetados.
- Evidencia: con `#pragma pack(1)` Clang describió un `int` tras un byte; la proyección lo diferió. Los campos naturalmente alineados, la reconstrucción de NSC, el contrato estático y la suite siguieron pasando. La comprobación es conservadora y no sustituye la futura corrección del lowering ni la validación de todos los accesos.
- Siguiente experimento: construir una tabla independiente de layouts de campos C++ a partir del índice validado, sin registrar un layout NativeSharp completo para el tipo. Permitir su paso a MIR solo mediante una segunda opción explícita para pruebas; comprobar primero lectura y escritura escalar en una instancia creada en C++, manteniendo bloqueada la ruta normal. Validar de nuevo tamaño y alineación antes de crear la tabla.
- Presupuesto: `ProjectObjectLayoutModel.h/.cpp`, `ProjectMemberModel.cpp`, `ProjectCompilation.h/.cpp`, `CppClangSessionTests.cpp`, fixture mixto, script de llamadas y bitácora. No habilitar construcciones, campos empaquetados ni propagación de excepciones.
- Evidencia inicial: en modo de pruebas, NativeSharp escribió `p->value = 19` y lo leyó; un método C++ de la misma instancia comprobó el nuevo valor y el proceso validó 110. La reconstrucción, suite y conjunto de llamadas mixtas pasaron. El campo estaba en offset 0; repetir con un desplazamiento no nulo antes de dar por probado ese aspecto.
- Resultado con desplazamiento no nulo: Clang informó offset 4 para `value`; NativeSharp lo escribió y leyó, y C++ comprobó tanto ese valor como que el campo anterior conservaba 42. El proceso validó 110 y terminó con código 0; la suite pasó. Solo se ha comprobado un campo `int` público y naturalmente alineado, con instancia creada por C++; el driver sigue bloqueado, y no se han probado las reglas de `const`, `volatile`, empaquetado, construcción ni destrucción.
- Siguiente comprobación: agregar campos escalares `const` y `volatile` al fixture. Exigir lectura de ambos, escritura observable del volátil y operaciones LLVM marcadas `volatile`; probar por separado que una escritura a `const` no sea aceptada. No extrapolar esta comprobación a memoria atómica o concurrencia.
- Resultado: en la ruta experimental se leen campos `const` y `volatile`, se escribe el volátil, C++ comprueba el nuevo valor y el ejecutable valida 165. El LLVM contiene `load volatile i32` y `store volatile i32`. Una compilación separada rechazó asignar al campo `const`; la primera aserción de la prueba falló porque esperaba el texto `readonly` y el diagnóstico real es `left side of assignment is not assignable`. Corregida la aserción, suite y ejecución pasaron. Falta comprobar toma de dirección y propagación de calificadores a punteros; no se ha demostrado seguridad de alias ni atomicidad.
- Protección de alias: `analyzeFieldMember` marcaba como direccionables los campos calificados, pero la representación de punteros aún no conserva aquí esos calificadores. Diferir la toma de dirección de campos C++ `const` o `volatile`, sin afectar los no calificados, hasta implementar punteros calificados. Presupuesto adicional: `MemberAccessAnalyzer.cpp` y pruebas de los tres casos.
- Evidencia: compilaciones separadas rechazaron obtener un `int*` de los campos C++ `const` y `volatile`, mientras `&p->value` para un campo ordinario llegó a MIR. Reconstrucción de NSC, contrato estático, suite y ejecución mixta pasaron. Es una restricción temporal de esta ruta, no soporte de punteros calificados ni garantía de que no haya otras formas de formar alias.
- Regresión: después de esta protección, los proyectos C y C++ con NSC de A: volvieron a dar 85 y 148 y `git diff --check` pasó.

## Proyección aislada de métodos estáticos C++

- Objetivo: conservar propietario, símbolo de Clang, firma y tipos escalares de un método estático público, no lanzador y de convención compatible con Windows x64. Es solo metadata; no incluirlo aún en el índice ni generar llamadas. Diferir las familias y características no soportadas sin retirarlas del objetivo.
- Presupuesto: `CppSemanticFunction.h/.cpp`, `CppClangSessionTests.cpp` y bitácora. Comprobar un método estático real, y rechazar métodos de instancia, privados, variádicos, con excepciones o convención diferente. No modificar el driver.
- Evidencia: Clang proporcionó `MyClass::B(double) noexcept` con símbolo `?B@MyClass@@SAHN@Z`; la proyección conservó propietario, retorno `int` y parámetro `double`. Se difirieron un método de instancia, uno privado, uno `noexcept(false)`, uno variádico y uno `vectorcall`. Reconstrucción de NSC, contrato estático y suite pasaron. No existe aún índice ni llamada para este resultado.
- Siguiente paso aislado: indexar el método estático como miembro estático propio del tipo, sin `this`, usando un contrato separado del método de instancia. Archivos: nuevo `CppImportedStaticMethod.h`, `ProjectMemberModel.h/.cpp`, `CppSemanticFunction.h/.cpp`, `CppClangSessionTests.cpp` y bitácora. Comprobar propietario, sobrecargas, símbolos y duplicados; sin activar aún el driver.
- Evidencia: un índice aislado instaló `B(double)` de Clang y una segunda sobrecarga sintética, ambas estáticas y sin `this`; rechazó un duplicado y un propietario incorrecto. La sobrecarga sintética solo prueba el índice, no resolución ni enlace. Reconstrucción de NSC, contrato estático y suite pasaron. Aún no hay proveedor de métodos estáticos en `ProjectCompilation` ni prueba de llamada.
- Siguiente paso: conectar un proveedor experimental desde sesiones reales. No publicar parcialmente una familia que mezcle métodos estáticos con otros métodos o incluya contratos no proyectables, y mantener la protección normal de generación. Presupuesto: `ProjectCompilation.h/.cpp`, `ProjectCompilationSnapshots.cpp`, `CppSemanticFunction.h/.cpp`, `CppClangSessionTests.cpp` y bitácora. El driver permanece sin cambios.
- Evidencia: un encabezado real produjo dos sobrecargas de `Demo.Widget.scale` como miembros estáticos sin `this`. La familia mixta `merge` no se publicó parcialmente, y una compilación independiente confirmó que los miembros se bloquean antes de generar código en la ruta normal. Se desactivaron los snapshots/cachés relevantes cuando este proveedor está activo, aún sin probar su combinación con módulos precompilados. Reconstrucción de NSC, contrato estático, suite y llamadas mixtas pasaron; todavía no se ha ejecutado una llamada estática.
- Siguiente experimento: activar explícitamente el proveedor solo en el arnés mixto y llamar desde NativeSharp a `Demo.Widget.scale` con `int` y `double`; definir ambas sobrecargas en C++ y comprobar sus resultados. No habilitar el driver ni presentar esta prueba como resolución completa de sobrecargas C++.
- Resultado: NativeSharp llamó las dos sobrecargas estáticas `scale(3)` y `scale(2.5)` implementadas en C++; cada implementación verificó su argumento y devolvió 12 o 13. El ejecutable mixto validó el resultado total 190; la suite y el conjunto de llamadas pasaron. Es una prueba de firmas escalares exactas, no de todas las conversiones o reglas de sobrecarga C++, y la ruta sigue limitada al arnés.

## Proyección aislada de campos estáticos C++

- Objetivo: preservar el símbolo real y los calificadores de campos estáticos públicos escalares, sin habilitar todavía su uso. La instantánea actual omite si el almacenamiento es `thread_local`, así que primero se conserva ese dato y se difiere TLS; tampoco se promete que una declaración tenga definición en el binario.
- Presupuesto: `CppRecordDeclaration.h`, `ClangSession.cpp`, `CppSemanticRecord.h/.cpp`, `CppClangSessionTests.cpp` y bitácora. Probar símbolos y calificadores reales de Clang, acceso privado y rechazo de TLS; no modificar el driver.
- Evidencia: Clang proporcionó el símbolo `?shared@MyClass@@2HA` y se preservaron `const` y `volatile` en otros campos estáticos. El campo privado y el `thread_local` se difirieron; la instantánea conserva el indicador TLS. Reconstrucción de NSC, contrato estático y suite pasaron. Aún no hay índice, proveedor o acceso para campos estáticos; TLS sigue pendiente.
- Siguiente paso: indexar la declaración estática por tipo y nombre conservando símbolo externo, sin inicializador ni almacenamiento propio en NSC. Presupuesto: nuevo `CppImportedStaticField.h`, `ProjectMemberModel.h/.cpp`, `CppSemanticRecord.h/.cpp`, `CppClangSessionTests.cpp` y bitácora. Verificar duplicados, propietario y calificadores en un índice aislado; no generar todavía código ni habilitar driver.
- Ajuste relacionado: `ProjectObjectLayoutModel.cpp` debe excluir los campos estáticos externos de la tabla de desplazamientos de instancia.
- Evidencia: el índice aislado conservó el símbolo de `MyClass::shared`, registró `limit` como `readonly` e `io` como `volatile`, rechazó un duplicado y un propietario incorrecto. La reconstrucción de NSC, el contrato estático del host y la suite pasaron. El índice no crea por sí mismo almacenamiento ni está conectado a la generación de código; falta implementar y verificar que MIR use el símbolo externo correcto.
- Siguiente paso: conectar proveedor experimental de campos estáticos y hacer que MIR los declare usando el símbolo externo de Clang, sin inicializador ni accessor NativeSharp. Mantener protección por defecto y permitir solo la activación explícita de pruebas de campos. Presupuesto adicional: `ProjectCompilation.h/.cpp`, `ProjectCompilationSnapshots.cpp`, `MirBuildCoordinator.cpp`, proveedor en `CppSemanticRecord.h/.cpp`, `CppClangSessionTests.cpp`, fixture mixto y bitácora. Probar lectura/escritura y que no se defina almacenamiento duplicado.
- Evidencia: el arnés escribió y leyó `Demo.Widget.shared` e `io`, leyó `limit const`, y C++ observó los mismos valores. LLVM declaró `?shared@Widget@Demo@@2HA` como global externo; el enlace con la definición C++ no produjo duplicados y el proceso validó 234. Una compilación separada confirmó que la protección normal detiene los campos estáticos antes de generar código. Reconstrucción de NSC, contrato estático, suite y llamadas mixtas pasaron. El driver no activa el proveedor; faltan otros tipos, alias calificados, TLS, inicialización global general y DLL.
- Siguiente comprobación: impedir también que las direcciones de campos estáticos `const` y `volatile` se conviertan en punteros sin calificadores; verificar que el campo estático ordinario siga siendo direccionable y que la escritura a `const` se rechace. Presupuesto: `CppClangSessionTests.cpp` y bitácora; corregir el análisis de miembros solo si la prueba encuentra una falla.
- Resultado: compilaciones independientes rechazaron la asignación a `Demo.Widget.limit` y la obtención de direcciones sin calificadores de `limit` e `io`; `&Demo.Widget.shared` sí llegó a MIR. La suite pasó sin modificar más el análisis de miembros. No demuestra soporte general de punteros calificados ni inicialización TLS.
- Regresión: con el NSC construido en A:, los proyectos C y C++ volvieron a dar 85 y 148; `git diff --check` pasó.

## Proyección aislada de constructores y destructores C++

- Objetivo: describir constructores y destructores públicos explícitos desde Clang, preservando símbolo y parámetros; inicialmente solo clases sin bases, firmas escalares, destructor no virtual y especificación no lanzadora. La proyección no reserva ni construye, no ejecuta limpieza y no implica soporte de herencia o excepciones.
- Presupuesto: `CppSemanticFunction.h/.cpp`, `CppClangSessionTests.cpp` y bitácora. Verificar símbolos Windows x64 reales, acceso privado, bases y contratos diferidos. Clang omite actualmente miembros implícitos del inventario; su generación se investigará aparte, no se interpretará la ausencia como falta de constructor o destructor C++.
- Evidencia: de Clang se proyectaron el constructor `PlainLifetime(int) noexcept` y el destructor `~PlainLifetime() noexcept` con símbolos `??0PlainLifetime@@QEAA@H@Z` y `??1PlainLifetime@@QEAA@XZ`. Se difirieron los miembros privados y los de una clase derivada. Reconstrucción de NSC, contrato estático y suite pasaron. No se ha emitido ni ejecutado ningún constructor o destructor desde NativeSharp; la firma descriptiva no demuestra por sí sola el ABI de esas llamadas.
- Investigación de ABI: compilar con Clang un ejemplo explícito de construcción en almacenamiento existente y destrucción de la misma clase, y examinar las declaraciones y llamadas LLVM emitidas. Presupuesto: fixture `cpp-lifetime-abi.cpp` bajo `pruebas-llamadas-escalares`, salida temporal en `A:/nsc-interop-build/cpp-fixtures` y bitácora; no enlazarlo al compilador todavía.
- Resultado Windows x64: Clang emitió el constructor `??0PlainLifetime@@QEAA@H@Z` con firma LLVM `ptr (ptr, i32)` y el destructor `??1PlainLifetime@@QEAA@XZ` con firma `void (ptr)`. En cambio, la ruta genérica actual de constructores NativeSharp usa retorno `void`; no debe reutilizarse sin representar correctamente la firma C++ o una adaptación validada. El IR está en `A:/nsc-interop-build/cpp-fixtures/lifetime-abi.ll`; es evidencia de esta clase y este target, no de todas las variantes de ABI.
- Investigación paralela de enlace dinámico: las instantáneas actuales tampoco conservan `__declspec(dllimport)`. Inspeccionar la salida de Clang para datos y funciones importados por DLL antes de reutilizar la ruta de símbolos de bibliotecas estáticas. Presupuesto: fixture `cpp-dll-abi.cpp`, IR bajo `A:/nsc-interop-build/cpp-fixtures` y bitácora; no modificar la conexión de producción.
- Resultado: para el target Windows x64, Clang marca tanto la variable como la función con `dllimport` en LLVM; la variable aparece como `external dllimport global i32`. La ruta experimental actual de campos estáticos emite `external global` sin esa marca y solo se ha validado con objetos enlazados directamente; no se debe afirmar soporte DLL para esos datos. La lectura de atributos, su propagación y una prueba real con DLL quedan pendientes. IR: `A:/nsc-interop-build/cpp-fixtures/dll-abi.ll`.
- Siguiente paso de observación: conservar `dllimport` de Clang en instantáneas de funciones libres, globales, métodos y campos estáticos. Presupuesto: `CppFunctionDeclaration.h`, `CppGlobalDeclaration.h`, `CppRecordDeclaration.h`, `ClangSession.cpp`, `CppClangSessionTests.cpp` y bitácora. No cambiar aún la emisión ni afirmar que el atributo del header, por sí solo, identifica todos los casos de DLL.
- Evidencia: las instantáneas de una función y una variable importadas, así como de un método y un campo estático de una clase `dllimport`, conservaron el atributo; reconstrucción de NSC, contrato estático y suite pasaron. El dato aún no llega al backend ni se han probado DLL reales.
- Protección transitoria: diferir declaraciones explícitamente `dllimport` en las proyecciones que aún no transportan ese atributo, para evitar generar por error una referencia ordinaria a datos importados. Esto no sustituye el soporte DLL; las declaraciones siguen en el inventario. Presupuesto adicional: `CppSemanticInventory.cpp`, `CppSemanticFunction.cpp`, `CppSemanticRecord.cpp` y pruebas.
- Evidencia: las proyecciones de una función libre, variable global, método de instancia y campo estático con `dllimport` se difirieron en la suite; las declaraciones ordinarias y el ejecutable mixto siguieron pasando. Reconstrucción y contrato estático también pasaron. Esto afecta temporalmente usos explícitos `dllimport`; sigue pendiente implementarlos de verdad y distinguirlos de símbolos sin atributo que procedan de una DLL.
- Siguiente implementación aislada: transportar `dllimport` de campos estáticos C++ hasta la declaración LLVM y comprobar lectura/escritura con una DLL real y su biblioteca de importación. Mantener el driver desactivado y las demás declaraciones DLL diferidas. Presupuesto: metadatos de campo estático, `ClassMemberSymbol.h`, `ProjectOopMirStaticField.h`, `ProjectMemberModel.cpp`, `CppSemanticRecord.h/.cpp`, `MirBuildCoordinator.cpp`, identidad de caché MIR, `OopLlvmStaticState.cpp`, `CppClangSessionTests.cpp`, fixtures y script de pruebas, y bitácora. No asumir que un header sin atributo no pueda referir una DLL.
- La primera reconstrucción falló porque la interfaz propia `src/LLVM/LlvmCApi.h` no declara `LLVMSetDLLStorageClass` ni sus constantes. Se amplía el presupuesto a esa interfaz, usando los valores de `llvm-c/Core.h` de la versión fijada; volver a construir y verificar el enlace real.
- Evidencia: con el proveedor experimental y la opción de prueba, NativeSharp escribió y leyó `Plugin.Imported.value` declarado `dllimport`; LLVM conservó el símbolo y lo declaró `external dllimport global i32`. El objeto enlazó con una DLL C++ real y su `.lib` de importación, y el proceso comprobó el valor 28. `llvm-readobj --coff-imports` confirmó la importación de `?value@Imported@Plugin@@2HA` desde `cpp-dll-field-library.dll`. Reconstrucción de NSC, contrato estático, suite y conjunto mixto pasaron. Sigue desactivado en el driver; funciones, métodos y globales libres `dllimport`, TLS, inicialización de DLL y headers sin el atributo siguen pendientes. No se han probado todas las formas de alias ni ownership entre módulos.

## Presupuesto: variable global libre importada de DLL

- Objetivo: conservar el atributo `dllimport` en variables libres escalares ya proyectables y emitir almacenamiento externo importado por LLVM. La declaración actual se rechaza en `projectCppSemanticGlobal`; el backend ya usa `cppDllImport` para campos estáticos. No se modifica TLS ni el soporte de funciones, métodos, inicialización o excepciones de DLL.
- Archivos y símbolos: `CppSemanticInventory.h/.cpp` (proyección, selección y proveedor), `NamespaceStateSymbol.h` (atributo), `MirBuildCoordinator.cpp` (transferencia a MIR), `CppClangSessionTests.cpp` (regresión y objeto mixto), fixture `cpp-dll-field-library.cpp` y esta bitácora; `TRACKING.md` solo para anotar evidencia parcial. Las declaraciones incompatibles del mismo símbolo deben diagnosticarse, no fusionarse. No crear almacenamiento duplicado.
- Validación: reconstruir NSC y la suite, comprobar el IR y enlazar y ejecutar con DLL real; confirmar imports y regresión de los proyectos existentes. Los archivos y características ajenos a este presupuesto quedan protegidos. Este paso no cierra la tanda ni certifica interoperabilidad general.
- Evidencia: la proyección conserva `dllimport`, la selección rechaza declaraciones con atributos contradictorios y MIR transmite el atributo al backend existente. La suite compiló código NativeSharp con la declaración de una variable escalar C++ importada y comprobó `external dllimport global i32` en LLVM. El objeto se enlazó contra una DLL real, escribió y leyó la variable y el proceso terminó con código 0; `llvm-readobj --coff-imports` muestra `?shared@Plugin@@3HA` desde `cpp-dll-field-library.dll`. La misma prueba usa un proveedor experimental para el campo estático de clase; no demuestra que ese proveedor esté activo en el driver. La reconstrucción y el contrato estático pasaron; las regresiones de proyectos C y C++ dieron 85 y 148; `git diff --check` pasó. No se ha verificado aún TLS, funciones `dllimport`, arranque DLL ni la ruta de un proyecto real con una variable importada.
- Siguiente verificación: proyecto NativeSharp real con `CppLibraryReference` sobre header y biblioteca de importación para una variable C++ `dllimport`; construir y ejecutar desde `A:/nsc-interop-build` con herramientas de `A:/al82dev-clang`. Presupuesto adicional: únicamente fixtures y script nuevos bajo `pruebas-proyecto`, tracking y esta bitácora; no cambiar el enlazador como parte de la prueba. Una falla se registra con su evidencia antes de proponer corrección.
- Evidencia de proyecto: `pruebas-proyecto/ejecutar-dll-global.cmd` construyó la DLL y su biblioteca de importación con Clang y LLD en A:, compiló con NSC el proyecto que referencia el header y la `.lib`, y ejecutó `Plugin.shared = 41; return Plugin.shared;` con resultado 41. `llvm-readobj --coff-imports` del ejecutable confirmó `?shared@Plugin@@3HA` en `cpp-dll-field-library.dll`. Esto verifica un escalar C++ mutable; no generaliza a TLS, inicialización de DLL, funciones ni otras formas de datos.

## Presupuesto: función libre importada de DLL

- Objetivo: transportar `dllimport` de una función C/C++ libre proyectable hasta LLVM, conservando el símbolo de Clang, y comprobar una llamada escalar `noexcept` con DLL real desde proyecto y arnés. Mantener los límites existentes de firma y ABI y no habilitar métodos DLL ni suponer soporte de excepciones.
- Archivos: `CppSemanticFunction.cpp`, `CppSemanticFunctionGroups.cpp`, `NamespaceFunctionSymbol.h`, `ProjectOopHirFunction.h`, `ProjectOopMirFunction.h`, `HirBuildCoordinator.cpp`, `MirFunctionLowerer.cpp`, `HirCacheIdentity.cpp`, `MirCache.cpp`, `ProjectOopMirCacheIdentity.cpp`, `OopLlvmDeclarations.cpp`, `CppClangSessionTests.cpp`, fixture de DLL, fixtures y script del proyecto DLL, tracking y bitácora. Preservar la identidad de caché y rechazar atributos contradictorios en redeclaraciones; no alterar funciones ordinarias.
- Pruebas: reconstrucción, suite y código LLVM; ejecutar contra DLL y confirmar imports en el binario; regresiones de los proyectos C y C++. No se declara completa la tanda por este caso.
- Evidencia: `dllImport` llega de Clang al símbolo semántico, HIR, MIR y a la declaración LLVM; se incorporó a las identidades HIR y de objeto y a la serialización y comparación de MIR. La selección diagnostica redeclaraciones contradictorias, cubiertas en la suite. En un arnés se verificó el atributo LLVM y se ejecutó la llamada; en `pruebas-proyecto/ejecutar-dll-global.cmd`, NSC compiló un proyecto que llama `Plugin.increment(40)` de una DLL real y devolvió 41. `llvm-readobj --coff-imports` confirma `?increment@Plugin@@YAHH@Z` y `?shared@Plugin@@3HA` en esa DLL. La reconstrucción, contrato estático, suite, llamadas mixtas, proyectos C y C++ (85 y 148) y `git diff --check` pasaron. La prueba del proyecto desactiva caché de artefactos: no demuestra una compilación incremental con caché. Tampoco demuestra propagación de excepciones ni métodos DLL.
- Siguiente verificación: proyecto NativeSharp con un header C, función escalar `dllimport`, DLL C real y biblioteca de importación. Presupuesto: fixtures y script nuevos en `pruebas-proyecto`, tracking y esta bitácora; no modificar NSC para hacer pasar la prueba. Conservar el resultado y la tabla de imports, incluso si falla.
- Evidencia: `pruebas-proyecto/ejecutar-dll-c.cmd` compiló `CDllApi.c` con Clang C11, creó DLL e import library con LLD y compiló el proyecto NS con `CppLibraryReference` en modo C. `c_increment(40)` devolvió 42; `llvm-readobj --coff-imports` identificó `c_increment` en `c-dll.dll`. El proyecto desactiva la caché de artefactos; esta prueba individual no demuestra todas las firmas C ni la inicialización de DLL.

## Presupuesto: identificar argumentos predeterminados C++

- Clang conserva si un parámetro tiene argumento predeterminado; `CppFunctionParameter` actualmente lo pierde. Registrar únicamente esa propiedad en snapshots de funciones y métodos. No evaluar expresiones, cambiar la resolución de sobrecargas ni habilitar argumentos omitidos.
- Archivos autorizados: `CppFunctionDeclaration.h`, `ClangSession.cpp`, `CppClangSessionTests.cpp`, `TRACKING.md` y esta bitácora. Comprobar una función y un método reales con parámetros con y sin valor por defecto, más regresión de la suite. El valor sigue pendiente aunque la marca exista.
- Evidencia: los snapshots de Clang conservaron `hasDefaultArgument` para el segundo parámetro, y no para el primero, en una función y un método C++ reales. La reconstrucción, el contrato estático de NSC, la suite y `git diff --check` terminaron correctamente. No se conserva la expresión ni se ha comprobado herencia de argumentos en redeclaraciones, evaluación ni llamadas con parámetros omitidos.

## Investigación: excepciones C++ en Windows x64

- Alcance: comparar la representación LLVM que Clang 23.1.0 emite para una excepción C++ real con las rutas de excepciones de NSC. Evidencia aislada, sin modificar ni habilitar tratamiento de excepciones. No asumir que el nombre del handler por sí solo demuestre interoperabilidad o limpieza.
- Presupuesto: fixture `cpp-exception-abi.cpp` en `pruebas-llamadas-escalares`, IR generado en `A:/nsc-interop-build/cpp-fixtures`, tracking y bitácora. Compilar con Clang de A: al triple fijado, inspeccionar el IR y anotar el siguiente experimento necesario. Sin cambios en NSC ni archivos en C:.
- Evidencia: Clang 23.1.0, `--target=x86_64-pc-windows-msvc -std=c++20 -O0 -S -emit-llvm`, emitió `_CxxThrowException` para `throw 42` y `__CxxFrameHandler3` como personalidad de la función con `catch (int)`, con `invoke` hacia el manejador. El IR está en `A:/nsc-interop-build/cpp-fixtures/cpp-exception-abi.ll`. En NSC, `OopLlvmControlFlow.cpp` instala `sehPersonalityFunction_` en regiones `try`; sus comentarios identifican `__C_specific_handler`. `OopLlvmCleanup.cpp` puede emitir `invoke` cuando existe un destino de desenrollado. Diferentes personalidades no prueban por sí solas que la propagación falle ni que se limpien recursos: faltan enlazado y ejecución cruzada con un destructor observable y control C++ equivalente. La proyección de funciones libres conserva `isNonThrowing` en el snapshot pero no lo consulta al proyectar; no se ha demostrado seguro el caso lanzador.
- Siguiente verificación: construir y ejecutar primero un control C++ aislado con `throw` y `catch` usando Clang/LLD de A: y las bibliotecas estáticas del runtime autorizado. Presupuesto: fixture `cpp-exception-control.cpp`, script correspondiente en `pruebas-llamadas-escalares`, artefactos en `A:/nsc-interop-build/cpp-fixtures/exception-control` y bitácora. No modificar NSC; registrar fallos de enlace como tales antes de intentar el cruce real.
- Primera ejecución: el enlace falló con símbolos de Win32 sin resolver, entre ellos `RaiseException`, `RtlUnwindEx` y `FlsAlloc`; la biblioteca `kernel32-prueba.lib` del arnés solo contiene imports de pruebas anteriores. Se localizó `A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib/um/x64/kernel32.Lib`. Se corregirá el script para usar esa biblioteca de importación del SDK autorizado y repetir el control; esto no demuestra aún ninguna conducta de excepciones.
- Control C++: usando `kernel32.Lib` del SDK de A:, el mismo script enlazó con `libcmt`, `libvcruntime` y `libucrt` y ejecutó `throw 42`/`catch(int)` con código 0. Esto confirma ese control, sin probar aún un frame NativeSharp.
- Siguiente prueba: compilar un objeto NativeSharp que llama una función C++ lanzadora dentro de `try/finally`; un arnés C++ atrapa la excepción y comprueba un contador incrementado desde el `finally`. La llamada del arnés a un símbolo NS es exclusivamente instrumento de prueba del desenrollado, no una funcionalidad C++ consumidor de NativeSharp. Presupuesto: fixtures y script nuevos en `pruebas-llamadas-escalares`, resultados en `A:/nsc-interop-build/cpp-fixtures/exception-cross` y bitácora. Mantener sin cambios NSC y registrar enlace, ejecución y resultado observable; no sustituye la prueba de destructor RAII.
- Primer intento cruzado: el driver rechazó `--no-artifact-cache` para `--emit-project-oop-obj` antes de compilar. `CompilerInvocation.cpp` indica que esta acción no admite ni activa la política de caché de artefactos del proyecto; se retira esa opción de la invocación y se conserva la salida en A:.
- Segundo intento: NSC generó el objeto; LLD identificó `CommandLineToArgvW` sin resolver desde `NativeSharp.Runtime.Win32.lib`. Se añade la biblioteca de importación `Shell32.Lib` del mismo SDK en A: y se repite el enlace.
- Resultado de `ejecutar-excepcion-cross.cmd`: con el objeto NativeSharp y runtime estático de C++ enlazados, `throw 42` atravesó la función NativeSharp, su `finally` incrementó el contador una vez y el arnés C++ atrapó el entero esperado; el proceso terminó con código 0. Es un caso O0 en un ejecutable, sin DLL ni objeto `raii`; no demuestra la adaptación de tipos de excepciones ni cobertura general.
- Siguiente prueba: reemplazar únicamente la fuente NS de este fixture por otra con un objeto `raii` cuyo destructor llama al contador; reutilizar el arnés C++ y enlazar otro ejecutable. Presupuesto adicional: `cpp-exception-raii.ns.txt`, script `ejecutar-excepcion-cross.cmd` y bitácora. Verificar una destrucción durante el desenrollado y mantener el caso `finally` original.
- Resultado O0: el objeto NativeSharp con `raii Guard` también se enlazó con el arnés C++; al lanzar `42`, el destructor llamó al contador exactamente una vez y el `catch(int)` del arnés capturó el valor. Continúa pendiente DLL, O2, objetos C++ y adaptación a excepciones NativeSharp. Siguiente comprobación: ejecutar ambos escenarios con el objeto NS a `-O2`, conservando el mismo arnés y fuentes; presupuesto limitado al script y bitácora.
- Resultado O2: el mismo script compiló los objetos NativeSharp `finally` y `raii` con `-O2`; ambos ejecutables también terminaron con código 0 y contador igual a uno. El arnés C++ permaneció a `-O0`. Esta evidencia no cubre excepciones lanzadas desde DLL ni tipos de excepción de usuario.
- Siguiente prueba: lanzar el mismo entero desde una DLL C++ con runtime estático y comprobar tanto `finally` como destructor NS en O0 y O2, usando el arnés únicamente como consumidor instrumental. Presupuesto: fixture de DLL, header de importación, script nuevo y un condicional en `cpp-exception-cross.cpp` para reutilizar el arnés; resultados en A: y bitácora. No modificar NSC ni inferir política general de ownership por el resultado.
- Resultado DLL: `ejecutar-excepcion-dll.cmd` construyó DLL y ejecutable con CRT estático y ejecutó los cuatro escenarios (`finally`/`raii` con NS O0/O2); todos terminaron con código 0, con excepción entera capturada y contador exactamente uno. `llvm-readobj --coff-imports` confirmó que `raii-2.exe` importa `cpp_cross_throw` de `cpp-exception-dll.dll`, y la DLL solo importa `KERNEL32.dll`. El arnés C++ usa O0 y la DLL usa O0; no se han probado excepciones de tipos C++ propios, captura nativa NS, callbacks ni transferencia de ownership entre runtimes.
- Siguiente prueba: repetir la matriz de DLL con una excepción de tipo C++ definido por usuario, atrapada por referencia en el arnés, comprobando también su campo. Presupuesto: header de tipo compartido, cambios condicionales en los dos fixtures C++, opción del script DLL y bitácora/tracking. Conservar el caso entero y no afirmar interoperabilidad general de excepciones por esta muestra.
- Resultado: `ejecutar-excepcion-dll.cmd tipo` lanzó desde la DLL `InteropFailure{42}`; los cuatro escenarios conservaron el tipo y el campo hasta `catch (const InteropFailure&)` y ejecutaron la limpieza NativeSharp exactamente una vez. Se volvió a ejecutar la variante entera; ambas pasaron. Las salidas se conservaron por separado en `exception-dll-type` y `exception-dll`. El tipo probado es un struct trivial; no cubre jerarquías de excepción, destructores C++ no triviales ni captura dentro de NativeSharp.
- Siguiente verificación: agregar una variante del mismo tipo con destructor C++ definido en la DLL y contador consultable después del `catch`; comprobar ejecución única además de la limpieza NS. Presupuesto: header/fixtures C++ y script DLL existentes, salidas independientes en A: y bitácora; no alterar el compilador ni generalizar el resultado a ownership arbitrario.
- Resultado: `ejecutar-excepcion-dll.cmd no-trivial` completó los cuatro escenarios; el arnés verificó que el destructor NS y el destructor del objeto de excepción C++ definido en la DLL se ejecutaron exactamente una vez. El valor `42` también llegó al `catch` por referencia. Sigue pendiente una contraprueba que demuestre que el instrumento detecta ausencia de limpieza NS; presupuesto adicional: fixture NS sin limpieza, script de prueba y bitácora.
- Contraprueba: el mismo arnés devolvió exactamente 2 cuando NativeSharp llamó a la función lanzadora sin `finally` ni `raii`; el script exigió ese resultado y detectó la ausencia de limpieza. Se repitieron por separado las variantes entero, struct trivial y struct con destructor para los cuatro escenarios positivos y el negativo; las tres ejecuciones terminaron con código 0. Persisten los límites de captura y ownership ya descritos.

## Presupuesto: métodos C++ importados de DLL

- Objetivo: preservar `dllimport` para métodos públicos de instancia y estáticos ya proyectables en el arnés experimental; validar sus símbolos y ejecución con una DLL real. El driver permanece desactivado; constructores, virtuales, excepciones y demás firmas siguen fuera de este incremento, sin reducir el objetivo general.
- Archivos: `CppImportedInstanceMethod.h`, `CppImportedStaticMethod.h`, `CppSemanticFunction.h/.cpp`, `ProjectMemberModel.cpp`, `HirBuildCoordinator.cpp`, `CppClangSessionTests.cpp`, fixture `cpp-dll-field-library.cpp`, tracking y bitácora. Se reutilizan transporte MIR y caché del atributo ya existentes; preservar ruta sin atributo. La prueba instala proveedores y guardias explícitas y debe comprobar el resultado real y `dllimport` en IR.
- Validación: reconstruir NSC y suite con Clang A:, ejecutar la suite y la prueba mixta DLL y verificar símbolos importados; ejecutar regresiones pertinentes. Si una familia contiene firma incompatible, conservar el rechazo; no habilitar el driver por esta prueba.
- Primera ejecución: compilación de NSC y suite pasaron; `llvm-nm` del objeto muestra los imports DLL de ambos métodos. El enlace del arnés sin runtime falló por `__nativesharp_trap`, generado para la comprobación de puntero nulo. El runtime lo declara como `void(int)` (`WindowsRuntimeBuilder.cpp`); se amplía el presupuesto a `cpp-dll-field-entry.cpp` para implementar en el arnés un trap que termina con código de error, y repetir el enlace. No se altera el runtime del producto.
- Evidencia: la suite verificó `dllimport` y los símbolos de `Plugin::Imported::scale(int)` y `add(int) const` en LLVM; el arnés obtuvo una instancia creada por C++, invocó ambos métodos de la DLL y produjo el resultado esperado. `llvm-readobj --coff-imports` confirmó `?scale@Imported@Plugin@@SAHH@Z` y `?add@Imported@Plugin@@QEBAHH@Z` desde `cpp-dll-field-library.dll`. El trap del arnés preserva la ruta de error ante puntero nulo, sin probar aquí un receptor nulo. La reconstrucción, contrato estático, suite, pruebas mixtas, regresión del proyecto C++ (148) y `git diff --check` pasaron. El driver no activa los proveedores experimentales; tampoco se verificaron familias de múltiples declaraciones DLL, otros ABI, tipos no escalares, excepciones ni métodos virtuales.
- Regresión adicional: tras modificar la DLL de prueba, los proyectos NativeSharp con biblioteca C estática y con variable/función C++ importadas desde DLL volvieron a ejecutar con resultados 85 y 41, respectivamente.

## Presupuesto: conectar clases y miembros C++ al driver

- Objetivo: hacer accesibles desde proyectos con referencias C++ los tipos y miembros ya proyectados y comprobados en el arnés, incluyendo punteros a records en funciones; conservar las restricciones existentes y no declarar compatibilidad total. Mantener C sin cambios de configuración y el rechazo de caché de artefactos para referencias C/C++.
- Archivos: `ProjectCompilation.h/.cpp` y `CppClangSessionTests.cpp` para nombrar apropiadamente las opciones de lowering; `PrecompiledModuleConfiguration.cpp` para instalar proveedores solo si existen sesiones C++; fixtures y script nuevos en `pruebas-proyecto`, tracking y bitácora. Sin modificar Clang, runtime ni despliegue en C:.
- Validación: suite aislada, proyecto real NativeSharp con método de instancia, método/campo estáticos y campo de instancia usando biblioteca C++ real, regresiones C y C++ y DLL, builds ON/OFF cuando corresponda, contrato estático y diff. Las firmas excluidas y la construcción/destrucción C++ permanecen protegidas.
- Primera ejecución: la compilación alcanzó la validación de bibliotecas y fue rechazada con `metadata-backed NativeSharp call resolved to unpublished symbol '?add@Widget@Demo@@QEBAHH@Z'`. `makePrecompiledRegularFunction` representa métodos C++ con `isImportedNative`; `validateImportedFunctionRuntime` interpreta todos esos métodos como NSMETA. Se amplía el presupuesto a `ProjectOopHirFunction.h`, `ProjectOopMirFunction.h`, `HirBuildCoordinator.cpp`, `MirFunctionLowerer.cpp`, `HirCacheIdentity.cpp`, `MirCache.cpp`, `ProjectOopMirCacheIdentity.cpp` y `LibraryReferenceValidation.cpp`: transportar identidad C++ explícita hasta validación y conservarla en cachés, sin relajar la validación de métodos NSMETA.
- Resultado inicial: `ejecutar-clase-cpp.cmd` compiló y ejecutó un proyecto NS real que llama `Demo.make()`, escribe/lee `p->value`, llama `p->add` y `Demo.Widget.scale` y lee `Demo.Widget.shared`, resultado 39. La suite, llamadas mixtas y regresiones de proyectos C (85), C++ (148), DLL C++ (41) y DLL C (42) pasaron; contrato estático pasó. La clase es un struct escalar y su objeto es creado por C++, no se ha demostrado construcción NS.
- Siguiente verificación: probar por la misma ruta de driver una clase C++ real en DLL y sus métodos importados. Presupuesto adicional: fixtures y script nuevos en `pruebas-proyecto` y bitácora, reutilizando la DLL `cpp-dll-field-library.cpp` ya validada. Verificar imports reales; no declarar cobertura de todas las clases ni lifetimes.
- Resultado DLL y regresiones: `ejecutar-clase-dll.cmd` compiló un proyecto NS real, obtuvo un objeto de la DLL, llamó `add` y `scale`, escribió/leyó `value` y devolvió 21. `llvm-readobj --coff-imports` confirmó ambos métodos, fábrica y campo en `cpp-dll-field-library.dll`. El proyecto estático se volvió a ejecutar y una fuente que intenta `new Demo.Widget` fue rechazada con diagnóstico de lifetime, sin generar ejecutable. `nsc_frontend` con integración C++ OFF construyó; `git diff --check` pasó. La validación NSMETA sigue aplicada a funciones no marcadas C++, pero no se ejecutó una regresión específica de metadatos precompilados en esta tanda. Clases complejas, construcción NS, métodos virtuales y errores de memoria siguen pendientes.
- Siguiente verificación: repetir los proyectos de clase estática y DLL a `-O2`, incluyendo el rechazo de construcción no implementada. Presupuesto limitado a sus scripts y bitácora; conservar pruebas por defecto.
- Resultado O2: `ejecutar-clase-cpp.cmd O2` devolvió 39 y volvió a rechazar `new Demo.Widget`; `ejecutar-clase-dll.cmd O2` devolvió 21. Se optimizó el código NS; las bibliotecas C++ de los fixtures se compilaron con sus opciones anteriores. Las mismas limitaciones sobre tipos y vida de objeto permanecen.

## Presupuesto: métodos C++ potencialmente lanzadores

- Objetivo: proyectar métodos públicos escalares de instancia/estáticos cuya especificación de excepciones no sea `noexcept`, conservando el resto de restricciones; verificar propagación real de excepción C++ y limpieza `raii` NS en O0/O2. Las pruebas previas ya muestran limpieza para funciones libres, pero no prueban métodos ni captura de C++ en NativeSharp.
- Archivos: `CppSemanticFunction.cpp`, `CppClangSessionTests.cpp`, fixtures y script nuevos en `pruebas-llamadas-escalares`, tracking y bitácora. No cambiar constructores/destructores, virtuales, filtros de excepciones ni la interpretación de `noexcept`. El arnés C++ se usa solo para observar la excepción al salir del frame NS.
- Validar suite, proyecto real objeto NS, método de instancia y estático realmente lanzadores en O0 y O2, limpieza observada, regresiones y contrato estático. Si no pasa, conservar evidencia y no presentar el cruce como compatible.
- Resultado: se retiró exclusivamente la restricción `isNonThrowing` de la proyección de métodos escalares públicos compatibles; se conserva la información de Clang y las demás restricciones, incluyendo lifetime. `ejecutar-excepcion-metodos.cmd` generó objetos NS mediante el driver y comprobó métodos de instancia y estáticos que lanzan `42`: el arnés C++ capturó el valor y observó exactamente una destrucción `raii` NS en O0 y O2. Reconstrucción ON, contrato estático, suite, llamadas mixtas y proyectos C/C++/clases estática y DLL pasaron; `git diff --check` pasó. No se probó un método lanzador en DLL, jerarquías, callback ni captura del tipo C++ por NativeSharp; no se ha cambiado el modelo de efectos NS.
- Siguiente verificación: compilar los mismos métodos lanzadores dentro de una DLL C++, importarlos mediante declaraciones `dllimport` y repetir instancia/estático con NS O0/O2 y contador `raii`. Presupuesto: fixtures `cpp-throw-method.h` y header DLL nuevo, script nuevo, outputs en A: y bitácora. Sin cambios al compilador ni generalización a otros contratos.
- Resultado DLL: `ejecutar-excepcion-metodos-dll.cmd` pasó los cuatro casos; el arnés capturó `42` tras una destrucción NS y `llvm-readobj` confirmó `?raise@Thrower@Demo@@QEAAHH@Z` y `?raiseStatic@Thrower@Demo@@SAHH@Z` importados de `methods.dll`. La variante enlazada estáticamente volvió a pasar después del cambio de fixture. DLL y arnés C++ usaron O0 y CRT estático; NS se probó en O0/O2. No demuestra captura C++ dentro de NativeSharp, otros tipos ni transferencia general de ownership.
- Siguiente prueba: modificar únicamente los métodos `add` y `scale` de los fixtures de proyecto y DLL para omitir `noexcept`, sin lanzar; ejecutar los proyectos en O0/O2. Presupuesto adicional: `CppClassApi.h/.cpp`, `CppDllClass.h`, `cpp-dll-field-library.cpp` y bitácora. El arnés previo conserva sus declaraciones `noexcept`, por lo que debe alinearse con la nueva firma antes de repetirlo; no se modifican métodos ajenos.
- Resultado: los proyectos completos con biblioteca estática y DLL, cuyos métodos ya no declaran `noexcept`, devolvieron 39 y 21 tanto en O0 como en O2. Se alineó la declaración del arnés en `CppClangSessionTests.cpp`; su reconstrucción, suite y llamadas mixtas pasaron. Estos casos no lanzan; las pruebas específicas anteriores observan la propagación de métodos que sí lanzan.

## Investigación: clase repetida en varios encabezados

- En la ruta de tipos `observeCppRecords` refina definiciones repetidas para transporte por puntero; los proveedores de miembros usan el agrupamiento inicial y aceptan solo `OneDefinition`. Hipótesis: dos referencias a headers que incluyen la misma definición permiten reconocer el tipo pero pierden métodos/campos. No asumir que equivalencia para punteros pruebe compatibilidad de miembros.
- Presupuesto inicial: fixtures y script nuevos en `pruebas-proyecto`, copiando la API y biblioteca ya probadas; resultados solo en `A:/nsc-interop-build`; bitácora. Reproducir la llamada con dos headers antes de modificar NSC y guardar el diagnóstico. Conservación de definiciones conflictivas y ausencia de cambios a C: son obligatorias.
- Reproducción: `ejecutar-clase-repetida.cmd` falla con cinco diagnósticos `has no compatible member named` para `value`, `add`, `scale` y `shared`. El mismo proyecto con una referencia devuelve 39. La selección de tipos refina el grupo, pero los cuatro proveedores de miembros omiten `MultipleDefinitionsUnverified`.
- Presupuesto correctivo: `CppSemanticRecord.h/.cpp` y `CppSemanticFunction.cpp` para refinar grupos y elegir una sola definición solo si Clang certifica equivalencia estructural, ODR hash/layout coinciden, las ubicaciones físicas de definición coinciden y todos los contratos de miembros capturados coinciden; `CppClangSessionTests.cpp` para casos compatibles y divergentes; fixtures/script del proyecto repetido y bitácora/tracking. Definiciones diferentes, desconocidas o sin evidencia siguen sin fusionarse. No interpretar esta verificación como ODR completo ni habilitar paso por valor.
- Primera corrección: métodos y campo estático dejaron de fallar, pero `value` siguió sin proyectarse porque `objectLayout` se omite intencionalmente en definiciones repetidas. Se amplía el presupuesto a `ProjectNameLookup.h/.cpp` y `ProjectObjectLayoutModel.cpp`: transportar dimensiones separadas exclusivas para validar campos por puntero, sin llenar `objectLayout` ni habilitar tipos por valor. Solo se suministran para definiciones que superan la comparación de miembros; dimensiones se validan igual que la ruta de una definición.
- La revisión adicional de `ProjectMemberModel.cpp` identificó una segunda validación que exige `objectLayout`; se amplía el presupuesto a ese archivo para que use las dimensiones de acceso por puntero bajo las mismas comprobaciones de offset, tamaño y alineación.
- El proyecto real con la misma definición en dos headers devolvió 39 y rechazó el header incompatible con `referenced C/C++ record has conflicting or unverified definitions`. La suite falló solo la prueba sintética de coincidencia de ubicación: se había usado una ruta de proyecto que no necesariamente existe en disco. Se ajustará el fixture sintético para señalar el ejecutable Clang real pasado al test; no cambia la comparación de producción.
- La suite corregida pasó y también las regresiones de proyectos C, C++ y clases. Antes de cerrar este incremento se verificará en el proyecto de dos headers que las nuevas dimensiones para miembros no permitan `new` ni insertar el record C++ por valor. Presupuesto adicional: fixture NS y script del proyecto repetido; no cambiar la protección de lifetime.
- Resultado: `ejecutar-clase-repetida.cmd` pasó el acceso a métodos y campos de la clase incluida por ambos headers, resultado 39; rechazó una definición incompatible y también `new` y un campo NS por valor del record importado. La selección compara origen físico, layout/ODR y contratos capturados, además de la comparación estructural de Clang; no certifica ODR completo ni cuerpos inline. Reconstrucción ON, contrato estático, suite, llamadas mixtas, regresiones de proyectos y `git diff --check` pasaron. La comparación se realiza actualmente por cada proveedor de miembros; costo y escalabilidad con headers grandes siguen pendientes de medición.

## Contrato físico de constructores y destructores C++

- Antes de conectarlos al modelo de construcción, conservar en la proyección de metadatos la diferencia del ABI Windows x64: constructor completo devuelve el puntero `this`; destructor completo devuelve `void`. Conservar también importación DLL y especificación no lanzadora, sin interpretar ninguno de esos datos como soporte de ejecución.
- Presupuesto: `CppSemanticFunction.h/.cpp`, `CppClangSessionTests.cpp` y este registro. La protección que impide construir y almacenar objetos C++ por valor permanece activa. Verificar la clasificación con la sesión Clang y casos de metadatos modificados, más regresiones existentes.
- Resultado 2026-11-04: la proyección para métodos explícitos de clases completas sin bases conserva `ThisPointer` para constructor y `Void` para destructor, así como `dllImport` y `isNonThrowing`. La prueba usa declaraciones reales de Clang para el caso ordinario y variantes sintéticas para comprobar que los dos indicadores se conservan; no demuestra construcción desde DLL ni propagación de excepciones de constructores. Reconstrucción de NSC y suite de sesiones, llamadas mixtas, proyecto de clase repetida y `git diff --check`: correctos. El contrato de `/MT` del compilador también pasó en la reconstrucción.
- Pendiente para habilitar uso: integrar candidatos de construcción en la resolución y el modelo de miembros; generar la llamada con retorno y símbolo correctos; reservar y alinear almacenamiento sin aplicar el encabezado de clase NativeSharp; conectar destructor y liberación compatibles con el propietario; manejar fallos de construcción y excepciones; comprobar el enlace y ejecución en biblioteca estática y DLL. No se ha habilitado `new` ni el almacenamiento por valor.
- Investigación adicional: la barrera existente cubre `new` de almacenamiento y locales por valor, pero el frontend tiene rutas separadas para `new T()` y `new (p) T()`. Se comprobarán ambas con proyectos reales antes de incorporar constructores: si alguna acepta el marcador de tipo C++ como un struct NativeSharp, se corregirá mientras no exista la integración completa. Presupuesto: fixtures/script del proyecto repetido, `ConstructionAndLifetimeExpressions.cpp` y este registro.
- Resultado: `new Demo.Widget()` terminaba rechazado por la regla de local por valor; `new (p) Demo.Widget()` llegaba a lowering y fallaba con `placement struct construction has no value layout`. Ahora ambas rutas comprueban explícitamente el tipo importado al analizar la expresión y producen un diagnóstico de lifetime. El código también contempla el ordinal de `new()` inferido por contexto, aún sin prueba específica. El proyecto con headers repetidos verificó los dos diagnósticos, además del acceso existente; el proyecto de clase con definición única devolvió 39. Reconstrucción, suite de sesiones, llamadas mixtas y `git diff --check`: correctos. Esto conserva una protección temporal, no implementa la construcción C++.
- Siguiente conexión: recolectar por proyecto y tipo las familias de constructores/destructores explícitos observados por Clang, con símbolos, ABI, argumentos y rechazos identificados; no escoger una sobrecarga si hay tipos no proyectables o colisiones de firma. Presupuesto: `CppSemanticFunction.h/.cpp`, pruebas de sesión/proyecto y este registro. La colección es preparación para el índice de miembros; no habilita todavía la construcción ni supone que los implícitos estén capturados.
- Resultado: `collectCppSemanticLifetimeFamilies` consulta las sesiones y el lookup de un proyecto, selecciona las definiciones con el mismo criterio de miembros existente y entrega las firmas explícitas por clase junto con motivos de proyección diferida y un indicador de colisión de firmas NativeSharp. La prueba con un header real de Clang comprobó constructor `int`, destructor, una sobrecarga con puntero diferida y la colisión entre `int` y `long` de 32 bits en Windows. Reconstrucción, suite, llamadas mixtas, proyecto de clase repetida y `git diff --check`: correctos. La colección todavía no está conectada al índice ni implementa selección de sobrecargas; no certifica constructores implícitos ni equivalencia ODR de definiciones repetidas.
- Conectar de forma aislada los miembros de lifetime explícitos al índice de `ProjectMemberModel`, conservando símbolos, origen DLL, parámetros y condición de excepción. Verificar que el índice de constructores los encuentra, que no confunde firmas o propietarios y que rechaza reconfiguración tardía. No conectar al driver ni retirar las protecciones: la resolución completa sigue dependiendo de miembros implícitos y sobrecargas diferidas. Presupuesto: `CppImportedInstanceMethod.h`, `ClassMemberSymbol.h`, `ProjectMemberModel.h/.cpp`, `CppSemanticFunction.h/.cpp`, suite de sesiones y esta bitácora.
- Resultado: una conversión transporta los metadatos de Clang al modelo de miembros y `setExperimentalCppLifetimeMethods` indexa constructores y destructores de tipos importados, con símbolo, parámetros, indicador DLL y condición de excepción. La prueba alimenta el modelo aislado con declaraciones reales, comprueba el índice de constructores, rechaza firmas duplicadas incluso con símbolos diferentes, otro proyecto como propietario y configuración posterior al build. Reconstrucción ON y contrato estático, suite, llamadas mixtas, clase con headers repetidos y `git diff --check`: correctos. La prueba selecciona explícitamente un subconjunto de una familia con colisión para observar solo el mecanismo del índice; no es una resolución válida de esa familia. El driver no utiliza aún esta ruta ni se habilitó construcción.
- Siguiente conexión: permitir que `ProjectCompilation` reciba un proveedor de familias y las incorpore a `ProjectMemberModel` antes de construirlo, únicamente por configuración experimental. El proveedor descartará la familia completa si contiene alguna declaración explícita diferida o colisión de firmas; una protección impedirá continuar a herencia y generación. Presupuesto: `ProjectCompilation.h/.cpp`, `ProjectCompilationSnapshots.cpp`, `CppSemanticFunction.h/.cpp`, suite y esta bitácora. La prueba usará proyectos con familias aptas y con ambigüedad, sin afirmar que se hayan capturado miembros implícitos.
- Resultado: `ProjectCompilation` admite un proveedor experimental, desactiva reutilización de snapshots/caché en esta modalidad y alimenta el índice de miembros. El proveedor no entrega una familia con métodos explícitos diferidos o firmas proyectadas en colisión. Una compilación de prueba con Clang indexa el constructor explícito de `Demo.Ready` y omite el de `Demo.Widget`, cuya familia tiene una sobrecarga diferida y otra colisión; el flujo se detiene antes de herencia. La primera prueba del bloqueo reutilizaba incorrectamente una compilación de un solo uso; se cambió por una instancia independiente y pasó. Reconstrucción ON, contrato estático, suite, llamadas mixtas, clase con headers repetidos y `git diff --check`: correctos. La modalidad no está activada en el driver y no demuestra resolución ni ejecución, tampoco enumera miembros implícitos.
- Inspección del siguiente obstáculo: `ClangSession::collectRecords` excluye intencionalmente métodos implícitos y Clang los declara de forma diferida. Se conservarán en cada record los indicadores de constructor predeterminado, copia, movimiento y destructor implícitos, tanto pendientes de declarar como ya creados por Clang, sin inventar sus firmas o símbolos. La familia de lifetime recibirá esos indicadores y la comparación de records repetidos los verificará. Presupuesto: `CppRecordDeclaration.h`, `ClangSession.cpp`, `CppSemanticRecord.cpp`, `CppSemanticFunction.h/.cpp`, suite y registro. La enumeración/instanciación de miembros implícitos y la resolución quedan pendientes.
- Resultado: los snapshots distinguen esas cuatro categorías y la familia de lifetime las conserva incluso si no hay métodos explícitos. Se comprobaron con un record cuyos miembros siguen diferidos, otro que los usa desde C++ y un tercero con constructor y destructor explícitos que suprimen las formas correspondientes. La comparación de definiciones repetidas incluye los indicadores. Reconstrucción ON, contrato estático, suite, llamadas mixtas, proyecto de clase repetida y `git diff --check`: correctos. Los indicadores describen presencia implícita, no si una operación está borrada, es accesible, ni si existe un símbolo enlazable; aún no constituyen resolución de sobrecargas.
- Se probará una consulta explícita por clase a `Sema::ForceDeclarationOfImplicitMembers` de Clang 23.1.0 para recoger tipo canónico, categoría y estado declarado/borrado/trivial, sin suponer que se haya emitido código enlazable ni interpretar una especificación de excepción sin resolver como `noexcept`. La consulta modifica el AST de esa sesión bajo su cerrojo; debe comprobarse su repetibilidad y que no cambie los snapshots usados para importar miembros explícitos. Presupuesto: `CppInteropSession.h`, `ClangSession.cpp`, suite y este registro; no conectar al driver.
- Resultado: `implicitLifetimeDeclarations` consulta Sema para una clase completa identificada por nombre, declara sus miembros especiales pendientes y devuelve categoría, tipo canónico, parámetros, acceso, borrado, trivialidad y estado de excepción resuelto o desconocido. Un caso con referencia sin inicializar comprobó que el constructor predeterminado implícito se marca borrado; otra clase confirmó los cuatro tipos de miembro y consultas repetidas. Para esa clase se comprobó que el ODR hash, los miembros explícitos y los indicadores previos permanecen iguales tras la consulta. Construcción ON, contrato estático, suite, llamadas mixtas, proyecto de clase repetida y `git diff --check`: correctos. Forzar declaraciones también puede declarar operadores de asignación y muta el AST bajo el mutex; no se ha demostrado invariancia para todas las clases. La consulta no genera cuerpos ni garantiza símbolos enlazables y todavía no participa en la resolución o el driver.
- Integrar la consulta de miembros implícitos en cada familia experimental asociándola a la sesión y referencia que proporcionó la definición seleccionada. Conservar explícitamente una consulta no disponible y no indexar esa familia; distinguirla de una respuesta válida sin miembros. Presupuesto: `CppSemanticFunction.h/.cpp`, suite y registro. No habilitar construcción ni asumir equivalencia de implementaciones implícitas entre distintas unidades.
- Resultado: las familias experimentales contienen ahora las declaraciones implícitas obtenidas de la sesión asociada al proyecto y referencia de la definición seleccionada. El proveedor omite la familia si no puede obtener esa observación, sin confundir indisponibilidad con un vector vacío. La suite comprobó un constructor de copia implícito junto con sobrecargas explícitas y usó una sesión controlada sin esa capacidad para verificar la omisión. Reconstrucción, suite, llamadas mixtas, clase con headers repetidos y `git diff --check`: correctos. Las declaraciones implícitas aún no se indexan ni se usan para resolver sobrecargas; el proveedor sigue restringido a observación experimental y no certifica equivalencia de los miembros implícitos entre unidades.
- Mejora paralela del flujo ya conectado: admitir parámetros y retornos de punteros no cualificados a escalares/void en métodos C++ de instancia y estáticos, reutilizando el mapeo de funciones libres. Mantener diferidos punteros a records y pointees `const`/`volatile` hasta transportar esos contratos. Presupuesto: `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`, fixtures nuevos bajo `pruebas-proyecto` y esta bitácora. Verificar compilación, enlace y ejecución reales para ambas formas de método; no modificar el ABI de las clases ni habilitar construcción.
- Resultado: los métodos C++ públicos no virtuales admitidos pueden recibir y devolver punteros no cualificados a escalares/void. El mapeo conserva el tipo de puntero y la validación del modelo de miembros lo acepta; los tipos a records y los pointees calificados siguen diferidos. El proyecto NativeSharp llamó un método de instancia y otro estático que modifican el mismo `int*`, en biblioteca estática y DLL, a O0 y O2: resultado 83 en los cuatro casos. `llvm-readobj --coff-imports` confirmó que el ejecutable DLL importa los dos símbolos C++ de `class-api.dll`. La prueba negativa de `const int*` fue rechazada en el análisis. Reconstrucción, contrato estático, suite de sesiones, llamadas mixtas, clase repetida y `git diff --check`: correctos. Solo se ejecutó `int*`; otras profundidades y tipos admitidos por el mapeo requieren verificación adicional. La construcción/destrucción de objetos C++ sigue pendiente.
- Extensión siguiente: transportar punteros no cualificados a records C++ en firmas de métodos de instancia y estáticos, únicamente cuando la identidad ya está presente en el lookup del proyecto y el grupo de definiciones permite transporte por puntero. Rechazar types ausentes o de otro proyecto; no habilitar paso por valor. Presupuesto: `CppSemanticFunction.h/.cpp`, `ProjectMemberModel.cpp`, fixtures de clases/punteros y registro. Verificar proyecto real en estática y DLL y conservar las regresiones.
- Resultado: los proveedores de métodos proyectan punteros a records C++ conocidos del mismo proyecto y el modelo valida que cada identidad importada esté presente en su lookup. El proyecto real pasa `Demo.Payload*` y lo recibe de métodos de instancia y estáticos; lee su campo y devuelve 94, tanto con biblioteca estática como DLL, a O0 y O2. Las pruebas del modelo rechazan un tipo de retorno inexistente para ambas clases de método. Reconstrucción, contrato estático, suite, llamadas mixtas, clase repetida y `git diff --check`: correctos. La prueba declara explícitamente `Demo.Payload*`; la propagación automática de dependencias de tipos en firmas de métodos no está demostrada. No se permite el paso de records por valor ni pointees calificados.
- Investigación y posible implementación: eliminar cualquier mención de `Payload` del programa NS y de las funciones libres, y obtenerlo únicamente por el retorno de un método C++. Comprobar enlace y ejecución con estática y DLL; corregir selección o inferencia si falta. Presupuesto: fixtures de punteros, `CppSemanticRecord.cpp`, proveedores/lookup o inferencia solo si el fallo los identifica y bitácora. No relajar aislamiento de proyectos ni colisiones para publicar tipos implícitos.
- Resultado: el flujo existente ya infiere `Payload*` a través de `Widget::getPayload` sin nombrar `Payload` en NativeSharp ni exponerlo como firma de función libre. El mismo programa lo pasa a métodos y lee un campo; devolvió 94 en estática y DLL a O0 y O2. Esto demuestra el escenario concreto; no prueba todas las dependencias transitivas ni los casos de colisión.
- Extender los métodos importados para transportar el calificador `const` inmediato del pointee en parámetros y retornos al contrato de punteros de NSC, manteniendo diferidos `volatile` y calificadores anidados no representables. Presupuesto: `CppImportedInstanceMethod.h`, `CppImportedStaticMethod.h`, `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`, fixtures de punteros y bitácora. Verificar lectura real y rechazo de escritura a través de un retorno `const`, tanto en métodos de instancia como estáticos, y las regresiones.
- Resultado: la proyección y el modelo de miembros conservan `const` inmediato del pointee en entradas y resultados de métodos de instancia y estáticos; se valida la correspondencia del indicador con parámetros puntero. El proyecto real encadena retornos `const int*` y `const Payload*`, lee ambos y devuelve 94 en biblioteca estática y DLL, a O0 y O2. Pruebas negativas rechazan modificar el entero o el campo del record mediante una variable inferida y pasar el retorno readonly de un método estático a un parámetro mutable. Reconstrucción ON, contrato estático, suite, llamadas mixtas, clase repetida y `git diff --check`: correctos. `volatile`, calificadores anidados y resolución de sobrecargas diferenciadas solo por constancia siguen pendientes; ante colisiones de firma proyectada se omite la familia.
- Implementar la selección para la familia concreta `f(T*)`/`f(const T*)` de un argumento en métodos C++ importados, tanto de instancia como estáticos: el argumento mutable exacto prefiere `T*`; el readonly solo admite `const T*`; `null` no debe seleccionar arbitrariamente. Mantener diferidas familias que colisionan por otras razones o requieren más reglas de C++. Presupuesto: `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`, `CallAnalyzer.cpp`, fixtures de punteros y bitácora. Comprobar estática, DLL, O0/O2 y la ambigüedad de null.
- Resultado: la selección de miembros conserva ambas firmas y aplica el desempate de constancia solo a métodos importados C++ de un argumento exacto, sin alterar el comparador general utilizado por funciones libres. En el proyecto real se llamaron versiones mutables y `const` de métodos de instancia y estáticos; devolvió 111 en biblioteca estática y DLL, a O0 y O2. Una llamada con `null` produjo diagnóstico de ambigüedad. También pasaron el contrato estático del compilador, suite de sesiones, llamadas mixtas, regresión de clase repetida y `git diff --check`. La regla no pretende resolver sobrecargas C++ generales, receptores `const` ni casos de varios parámetros.
- Siguiente capacidad: indexar pares de métodos C++ de instancia que solo difieren en `const` del receptor y decidir entre ellos según el receptor mutable o readonly, sin modificar la resolución de miembros NativeSharp. El rechazo readonly debe participar antes del desempate y no solo después de elegir. Presupuesto: `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`, `CallAnalyzer.cpp`, fixtures de punteros y registro; verificar llamadas reales y rechazo de mutación del receptor, estática y DLL, O0/O2.
- Resultado 2026-11-05: se conservan ambas declaraciones de `select()` y `select() const`; la llamada con receptor mutable selecciona la primera y la llamada con receptor readonly selecciona la segunda. La compilación, enlace y ejecución de la prueba produjeron 121 con biblioteca estática y DLL, tanto en O0 como en O2. La llamada readonly a `mutateOnly()` se rechazó con diagnóstico de sobrecarga no viable. También pasaron la suite de sesiones Clang, las llamadas mixtas, la prueba de clase repetida y `git diff --check`. Este resultado no demuestra la resolución general de sobrecargas ni combinaciones de constancia del receptor y de varios argumentos. Los cambios permanecen sin commit.
- Siguiente verificación: comprobar la interacción de constancia del receptor y de un único argumento puntero en métodos C++ importados, incluyendo preferencia y ambigüedad cruzada. Alcance: fixtures de `pruebas-proyecto` y esta bitácora; código de producción protegido hasta identificar un fallo concreto. Preservar llamadas ya verificadas y probar estática y DLL en O0/O2. No comprende resolución general de sobrecargas.
- Fallo reproducido: las cuatro sobrecargas `mix(T*)`, `mix(const T*)`, `mix(T*) const` y `mix(const T*) const` con receptor y argumento mutables producen ambigüedad tanto en estática como en DLL. La comparación local de `chooseNonDominatedCallCandidate` solo ordena cada dimensión cuando la otra es idéntica y no reconoce que la primera domina a la cuarta. Corrección autorizada por la tarea: comparar ambas dimensiones conjuntamente en ese punto, solo para métodos C++ de instancia con un argumento puntero exacto; conservar la ambigüedad si cada candidato es mejor en una dimensión. Archivo adicional: `src/Frontend/Bodies/CallAnalyzer.cpp`.
- Resultado 2026-11-05: la comparación combina las dos dimensiones para candidatos viables del mismo método y tipo, con un argumento puntero exacto. Las cuatro llamadas seleccionaron las cuatro variantes y devolvieron en conjunto el resultado esperado 136 en biblioteca estática y DLL, en O0 y O2. El par cruzado `cross(T*) const` / `cross(const T*)` con receptor y argumento mutables se rechazó como ambiguo en O0 y O2. Reconstrucción ON, contrato de runtime estático, suite de sesiones, llamadas escalares mixtas y `git diff --check`: correctos. No se acredita resolución general con conversiones distintas, varios argumentos, herencia ni plantillas. Los cambios continúan sin commit.
- Siguiente paso de lifetime: conservar el nombre ABI que Clang asigna a las declaraciones implícitas con visibilidad de enlace externa y no eliminadas, con las mismas variantes de constructor y destructor usadas para declaraciones explícitas. Este nombre es informativo: no demuestra que exista cuerpo ni símbolo enlazable y no habilita construcción ni cambia el driver. Archivos: `CppInteropSession.h`, `ClangSession.cpp`, suite de sesiones y bitácora. Verificar constructor/destructor implícitos, miembros eliminados y repetición de consultas con Clang 23.
- Resultado 2026-11-05: la sesión expone `abiSymbol` opcional para miembros implícitos con visibilidad externa no eliminados; se reutilizan las variantes ABI de las declaraciones explícitas. La suite comprobó los nombres del constructor predeterminado y destructor de `BaseA`, la ausencia de nombre para un constructor eliminado y estabilidad al consultar dos veces. Construcción ON y contrato de runtime estático, suite de sesiones, regresiones de clases con biblioteca estática y DLL, y `git diff --check`: correctos. La primera construcción señaló inicializador omitido; se inicializó explícitamente y la reconstrucción terminó sin esa advertencia. No se ha generado cuerpo, demostrado enlace, resuelto overloads implícitos ni habilitado lifetime; sigue siendo una capacidad experimental. Siguiente acción: investigar la generación controlada del cuerpo requerido desde Clang y su integración con selección, acceso y excepciones, antes de permitir construcción.
- Investigación siguiente: comparar los símbolos emitidos por Clang 23 para una clase con constructor y destructor implícitos no triviales, al incluir el header sin uso y al solicitar construcción y destrucción explícitamente, en O0 y O2; examinar también `-femit-all-decls`. Guardar fuentes, comandos y salidas bajo `nativesharp-interop/pruebas-lifetime` y `A:/nsc-interop-build/cpp-fixtures/lifetime-emission`. No modificar el compilador ni habilitar construcción hasta tener evidencia de generación y enlace.
- Extensión de la misma investigación: enlazar y ejecutar las funciones de construcción y destrucción generadas por Clang, observando inicialización y destructor mediante un contador independiente del CRT. No se considera integración NativeSharp ni una propuesta final de interfaz.
- Evidencia 2026-11-05: `pruebas-lifetime/ejecutar.cmd` compiló, enlazó y ejecutó los casos O0/O2 correctamente usando Clang y LLD de A:. El constructor inicializó el campo a 17 y el destructor incrementó el contador una vez. Sin uso, el objeto no contiene símbolos de `Parent`; `-femit-all-decls` tampoco los produjo en este caso. Con uso, O0 contiene `??0Parent@@QEAA@XZ` y `??1Parent@@QEAA@XZ`, mientras O2 incorpora sus cuerpos y solo conserva las funciones externas de la prueba. Listados y objetos están en `A:/nsc-interop-build/cpp-fixtures/lifetime-emission`. Consecuencia limitada a este caso: conocer el nombre ABI no garantiza una definición enlazable, y la emisión automática debe considerar el nivel de optimización. Las funciones externas son un instrumento experimental; no se ha decidido que sean la solución final ni se ha integrado la construcción en NSC.
- Siguiente paso: indexar experimentalmente en el modelo de miembros el constructor predeterminado y destructor implícitos observados por Clang cuando son públicos, no eliminados y tienen nombre ABI; conservar marca de origen implícito y trivialidad. Una especificación de excepción sin resolver no se marca como no lanzadora. Los miembros de copia/movimiento, cuerpos y enlace siguen pendientes; el driver y la protección de construcción no cambian. Archivos: `CppImportedInstanceMethod.h`, `ClassMemberSymbol.h`, `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`, suite y bitácora. Verificar miembro implícito, origen, símbolo, omisión de eliminado y permanencia de la protección.
- Resultado 2026-11-05: el proveedor experimental incorpora constructor predeterminado y destructor implícitos que cumplen esas condiciones; transporta además la marca DLL observada por Clang. En el modelo permanecen identificados como implícitos y triviales cuando corresponde. La suite con declaraciones reales verificó ambos símbolos de `Demo.Implicit`, sus marcas, la omisión del constructor predeterminado eliminado de `Demo.Deleted` y la protección que impide avanzar a generación para este proveedor. Reconstrucción ON, contrato estático, suite, llamadas escalares, pruebas de clases estática/DLL y `git diff --check`: correctos. El nombre ABI no demuestra que exista cuerpo; no se habilitó construcción, copia, movimiento ni destrucción ejecutable. Los cambios siguen sin commit.
- Siguiente paso: pedir a Clang Sema que resuelva especificaciones de excepción implícitas aún no evaluadas, sin asumir que toda operación es `noexcept`. Mantener `Unresolved` cuando no se pueda obtener resultado y no evaluar declaraciones eliminadas. Archivos: `ClangSession.cpp`, suite y bitácora; preservar la protección contra ejecución. Verificar una clase trivial no lanzadora, una que contiene miembros con constructor y destructor potencialmente lanzadores, consulta repetida e indexación experimental.
- Resultado 2026-11-05: la sesión solicita a Sema resolver la especificación `EST_Unevaluated` de miembros implícitos no eliminados y conserva `Unresolved` si no obtiene un resultado resuelto. La suite verificó que constructor y destructor implícitos triviales son no lanzadores, que una clase cuyos miembros pueden lanzar tiene ambas operaciones potencialmente lanzadoras y que las consultas repetidas conservan esa información. También confirmó la marca no lanzadora en el modelo experimental. Construcción ON, contrato estático de `nsc.exe`, suite, regresiones de clases estática/DLL y `git diff --check`: correctos. No demuestra captura ni propagación de excepciones durante construcción o destrucción; ambas operaciones siguen protegidas. Cambios sin commit.
- Siguiente capacidad productiva: campos públicos de instancia C++ que contienen punteros de un nivel a escalares/void, con tamaño y alineación de Clang. Distinguir constancia del puntero de constancia de lo apuntado; rechazar pointee volatile y formas no representadas. Archivos: `CppSemanticRecord.h/.cpp`, `CppImportedInstanceField.h`, `ProjectMemberModel.cpp`, fixtures de `pruebas-proyecto`, suite y registro. Verificar lectura, escritura permitida, rechazo de escritura a pointee const y a puntero const, con biblioteca estática y DLL, O0/O2; no habilitar construcción de objetos.
- Resultado 2026-11-05: se proyectan campos puntero de un nivel a escalares/void y se transporta separadamente la constancia del campo y del elemento apuntado; el cálculo de layout valida los 8 bytes del puntero para el triple establecido. Un proyecto real con campos `int*`, `const int*` e `int* const` leyó y escribió los valores permitidos, incluido escribir a través de un puntero const o de un objeto const cuyo puntero apunta a dato mutable; devolvió 139 con estática y DLL en O0/O2. El arnés estático rechazó escribir al pointee const, reasignar el puntero const y asignar un puntero a const al campo mutable. El primer diagnóstico de asignación usó la palabra `mutability`, por lo que se añadió al verificador y se repitió. Suite de sesiones, llamadas escalares y `git diff --check`: correctos. `void*`, pointee volatile, punteros múltiples y campos estáticos puntero no fueron probados como parte de esta capacidad; la construcción de objetos sigue protegida. Cambios sin commit.
- Siguiente capacidad productiva: campos estáticos C++ de un nivel de puntero a escalares/void, preservando el símbolo de enlace y la constancia del puntero y del pointee. Mantener excluidos pointees volatile, TLS y tipos no representados. Archivos: `CppSemanticRecord.h/.cpp`, `CppImportedStaticField.h`, `ProjectMemberModel.cpp`, fixtures `pruebas-proyecto` y registro. Verificar acceso y modificación reales con biblioteca estática y DLL, O0/O2, además de rechazos por pérdida de const.
- Resultado 2026-11-05: se proyectan campos estáticos puntero a escalares/void con símbolo externo, marca DLL y constancias independientes del campo y pointee. Un proyecto real accedió a `int*`, `const int*` e `int* const`, reasignó el campo mutable y modificó el dato a través del puntero const: resultado 140 con estática y DLL, O0 y O2. El arnés de biblioteca estática rechazó escribir al dato const, reasignar el puntero const y descartar constancia al asignar. Reconstrucción ON, contrato estático, suite de sesiones, llamadas escalares y `git diff --check`: correctos. No se probó `void*`, ni se implementaron TLS, pointees volatile o punteros múltiples. Construcción de objetos C++ continúa pendiente. Cambios sin commit.
- Siguiente capacidad: importar campos de instancia y estáticos de un nivel de puntero a un record C++ ya visible en el proyecto, preservando identidad nominal y constancias; comprobar visibilidad del tipo apuntado antes de indexarlo. Archivos: `CppSemanticRecord.h/.cpp`, `ProjectMemberModel.cpp`, fixtures `pruebas-proyecto` y bitácora. Verificar acceso encadenado, asignación y rechazo de escritura a través de pointee const, en estática/DLL y O0/O2. La inferencia del record cuando solo aparece en el campo se mantiene pendiente; no habilitar construcción por valor.
- Resultado 2026-11-05: los campos de instancia y estáticos a un record C++ ya importado conservan su identidad nominal y constancia; el modelo comprueba que el tipo apuntado pertenece al proyecto. El proyecto real enlazó, ejecutó acceso encadenado, asignó punteros y leyó valores a través de campos const; devolvió 141 con biblioteca estática y DLL en O0 y O2. En el arnés estático se rechazaron escritura al record const y asignación que descartaría constancia, tanto para campos de instancia como estáticos, en O0 y O2. Construcción ON, suite de sesiones, llamadas escalares y `git diff --check`: correctos. El record usado ya era visible por otras declaraciones: su descubrimiento solo por el tipo de campo sigue pendiente; no se habilitó construcción ni paso por valor. Cambios sin commit.
- Siguiente verificación: comprobar con una clase C++ cuya identidad aparece solamente como tipo de campos de otra clase si el selector ya la incorpora para acceso encadenado desde NativeSharp. Añadir fixture de clase interna con campos de instancia y estático a ese tipo, sin funciones que lo devuelvan ni referencias explícitas de su nombre en NativeSharp; verificar lectura y escritura reales en estática/DLL, O0/O2. Código de producción protegido hasta detectar una falla.
- Evidencia 2026-11-05: `Demo::Hidden` aparece como tipo de `Payload::hidden` y `Widget::sharedHidden`, sin figurar en firmas de funciones ni escribirse su nombre en NativeSharp. El proyecto compiló, enlazó, leyó, modificó y reasignó estos campos correctamente con biblioteca estática y DLL en O0 y O2; el resultado permaneció 141. El selector ya agrega grupos de records representables aunque no estén expresamente demandados. La limitación señalada anteriormente como pendiente no se reproduce en este caso, por lo que no requirió modificación de producción. Permanecen sin verificar colisiones, tipos no representables y otras configuraciones.
- Siguiente investigación implementada: exponer solo en la sesión experimental la generación de IR LLVM por Clang para un constructor predeterminado o destructor implícito, público y no eliminado, usando Sema y CodeGen sobre su AST. Rechazar entradas ambiguas, tipos dependientes y bases virtuales; conservar las protecciones de construcción y no integrar aún el IR al driver. Archivos: `CppInteropSession.h`, `ClangSession.cpp`, `CMakeLists.txt`, suite y bitácora. Verificar IR de ambos miembros, rechazo de constructor eliminado y consultas inválidas, junto con la reconstrucción y el contrato estático.
- Resultado 2026-11-05: la sesión experimental materializa el cuerpo mediante Sema y produce un módulo LLVM mediante `clangCodeGen`; LLVM verifica cada módulo antes de devolver su IR. La suite comprobó definiciones —no solo declaraciones— de constructor y destructor implícitos de `BaseA` y de `ImplicitThrowing`; en este último aparecen referencias a los miembros no triviales correspondientes. Una segunda solicitud del constructor produjo el mismo IR; constructor eliminado, tipo inexistente y solicitud de copia se rechazaron. Reconstrucción ON y contrato de runtime estático, suite, regresiones de campos con biblioteca estática/DLL y `git diff --check`: correctos. No se ha producido ni enlazado un objeto desde este IR, ni conectado al driver, ni verificado ejecución, excepciones en ejecución o clases con bases virtuales. El bloqueo de construcción de objetos permanece. Cambios sin commit.
- Siguiente verificación: persistir el IR experimental del constructor y destructor no triviales generado en la suite, compilarlo a objetos con Clang oficial y enlazarlo con una implementación C++ separada de sus miembros y un ejecutable de prueba. Observar la construcción y destrucción mediante valores/contador en O0/O2. Archivos: suite, fixtures y script bajo `pruebas-lifetime`, bitácora; resultados bajo `A:/nsc-interop-build/cpp-fixtures/lifetime-emission`. No modificar el driver ni retirar las protecciones.
- Fallo observado: el IR contiene la definición `linkonce_odr` del miembro solicitado; al compilar el módulo aislado en O2 Clang elimina ese símbolo como no referenciado, y LLD no puede enlazarlo desde otro objeto. En O0 sí aparece. Corregir únicamente la emisión experimental para que la definición solicitada tenga enlace `weak_odr` y mantenga COMDAT, conservando deduplicación y permitiendo su emisión independiente; confirmar con inspección y enlace O0/O2. Archivo adicional: `ClangSession.cpp`. Revisar también que el script no informe éxito si falla un comando.
- Resultado 2026-11-05: el módulo experimental cambia únicamente la definición solicitada de `linkonce_odr` a `weak_odr`, conservando COMDAT; `llvm-nm` confirmó los símbolos de constructor y destructor en objetos O2. La suite genera de nuevo los IR y `pruebas-lifetime/ejecutar.cmd` los compila y enlaza, con dos copias de cada definición, contra miembros C++ implementados aparte; el ejecutable comprobó una construcción y una destrucción en O0 y O2. Reconstrucción ON, contrato estático, regresiones de campos estática/DLL y `git diff --check`: correctos. En la primera ejecución, el contenedor de comandos informó código cero pese al error de LLD y el script no imprimió su línea final; por ello se verificó explícitamente que la ejecución final alcanzara la línea de éxito, además de inspeccionar símbolos. No se ha diagnosticado completamente esa discrepancia de código de salida. La integración del objeto al driver, la invocación desde NativeSharp y el manejo de excepciones en ejecución siguen pendientes; el bloqueo de construcción permanece. Cambios sin commit.
- Siguiente verificación: reproducir la propagación de un fallo dentro de un bloque `for` del script de lifetime mediante un modo de autocomprobación explícito; preservar la ruta normal y registrar si el proceso invocador recibe fallo. Archivos: script y bitácora. A continuación continuar con la integración de objetos generados.
- Resultado 2026-11-05: `ejecutar.cmd --check-exit` provoca un error dentro del bloque; esta herramienta informó código 0 al invocar directamente el archivo por lotes, mientras `call ejecutar.cmd --check-exit` informó código 1, como corresponde. `call ejecutar.cmd` completó la prueba real con código 0 y la línea final de éxito. La discrepancia está reproducida en el modo de invocación de esta herramienta; usar `call` para las siguientes pruebas por lotes y verificar igualmente su salida.
- Siguiente implementación: permitir que el backend LLVM de NSC lea IR textual verificado y lo convierta a un objeto con su `LlvmTargetMachine`, para eliminar la dependencia del proceso externo de Clang en esta etapa. Añadir una prueba que use el IR de lifetime de la sesión, emita objetos en A: con niveles O0/O2 y los enlace/ejecute con el arnés ya existente. Mantener esta ruta experimental sin conectarla al driver ni habilitar construcción. Archivos: `LlvmModule.h/.cpp`, `LlvmCApi.h`, configuración LLVM si es necesaria, suite de sesiones, script de lifetime y bitácora.
- Resultado 2026-11-05: `LlvmModule::parseText` crea un contexto, analiza el IR con LLVM y verifica el módulo, liberando recursos y devolviendo diagnóstico ante IR inválido. La suite usó `LlvmTargetMachine` de NSC para producir objetos del constructor y destructor implícitos no triviales, con niveles de generación O0 y O2, sin proceso Clang para esos objetos; un IR inválido fue rechazado. El script invocado con `call` enlazó esos objetos con miembros C++ y copias COMDAT adicionales, y ejecutó construcción/destrucción correctamente en ambos niveles. Reconstrucción ON, contrato estático, regresiones estática/DLL, llamadas escalares y `git diff --check`: correctos. O2 aquí es nivel de generación de máquina, no evidencia de haber aplicado el pipeline de optimización de IR de NSC. La selección automática, enlace por el driver y uso directo desde NativeSharp permanecen pendientes. Cambios sin commit.
- Siguiente implementación: convertir la secuencia de la prueba en un emisor reutilizable del backend de NSC, que reciba sesión Clang, tipo, miembro implícito, target y ruta de objeto; validar Windows x64 y devolver errores de selección, lectura y emisión. La suite debe usar ese mismo emisor para el enlace y ejecución existentes y rechazar tipo inexistente. Archivos: emisor nuevo en `include/nativesharp/LLVM` y `src/LLVM`, `CMakeLists.txt`, suite y bitácora. No conectar aún al driver ni retirar protecciones de lifetime.
- Resultado 2026-11-05: `emitCppImplicitLifetimeObject` encapsula solicitud a la sesión, análisis/verificación del IR y emisión con el backend de NSC; valida triple Windows x64, nombre y ruta absoluta, e informa errores. La suite usa el emisor para los objetos de constructor y destructor, comprueba el rechazo de tipo inexistente y el arnés enlazó y ejecutó esos objetos en O0/O2 con copias COMDAT. Reconstrucción ON y contrato estático, compilación de la biblioteca `nsc_llvm` con interoperabilidad OFF, regresiones de proyectos estática/DLL y `git diff --check`: correctos. La compilación OFF verificó la biblioteca, no un ejecutable completo. El driver aún no selecciona ni enlaza automáticamente los objetos y la construcción NativeSharp permanece protegida. Cambios sin commit.
- Siguiente implementación: construir un plan experimental de emisión desde ordinales de miembros de lifetime ya seleccionados por el modelo; resolver tipo, sesión dueña y nombre C++ a partir del inventario y verificar el símbolo observado en Sema. Deduplicar solicitudes; rechazar ordinales inválidos, miembros explícitos o incompatibles, sesiones ambiguas y definiciones no únicas. El plan conserva referencias no propietarias a sesiones vivas y no se conecta al driver todavía. Archivos: `CppSemanticFunction.h/.cpp`, suite y bitácora. Probar selección de constructor/destructor implícitos, repetidos y solicitudes inválidas, además de emisión con la sesión seleccionada.
- Resultado 2026-11-05: `planCppImplicitLifetimeEmission` verifica los miembros seleccionados, exige una única definición, localiza su sesión de proyecto, contrasta la declaración y símbolo con Sema y deduplica ordinales; las referencias a sesión no transfieren propiedad. La suite seleccionó constructor y destructor implícitos de `Demo::Implicit`, eliminó una solicitud repetida, emitió un objeto mediante la sesión elegida y rechazó un miembro explícito y un ordinal inválido sin alterar el plan anterior. Construcción ON, contrato estático, prueba de lifetime con enlace/ejecución, regresiones estática/DLL y `git diff --check`: correctos. El rechazo de sesiones ambiguas y definiciones múltiples se implementó, pero no se verificó específicamente en esta prueba. Falta obtener automáticamente estos ordinales desde construcciones/destrucciones reales, conectar el plan al driver y completar lifetime. Cambios sin commit.
- Siguiente conexión experimental: derivar solicitudes implícitas a partir de los `ObjectConstructionBinding` y `ClassDestructionPlan` ya resueltos, sin interpretar el default nativo de NS como constructor C++ ni seleccionar candidatos no resueltos. Pasar los ordinales al plan que valida origen y sesión; no conectar al driver ni retirar protecciones. Archivos: `CppSemanticFunction.h/.cpp`, suite y bitácora. Como el frontend aún impide construir objetos C++, probar bindings controlados sobre miembros reales y dejar explícito que no son construcciones reales ejecutadas.
- Resultado 2026-11-05: `planCppImplicitLifetimeFromBindings` deriva miembros de construcciones y planes de destrucción, comprueba sus propietarios y rechaza selección por default nativo sin constructor C++ o destrucción sin destructor; delega deduplicación y verificación de sesión al plan anterior. La suite usó bindings controlados con miembros reales de Clang y confirmó dos solicitudes sin duplicados, rechazo del default nativo y del destructor ausente. Las cadenas de destructores de otros tipos se rechazan por ahora; herencia queda pendiente. Reconstrucción ON, contrato estático, prueba de lifetime con enlace/ejecución, regresiones estática/DLL y `git diff --check`: correctos. No se han producido bindings de construcción C++ reales ni se conectó el plan al driver; la construcción sigue protegida. Cambios sin commit.
- Siguiente paso experimental: permitir solo a pruebas analizar una expresión NativeSharp real `new Demo.Implicit()` hasta el modelo de construcciones con proveedor de lifetime, manteniendo una barrera explícita antes de layout/HIR y sin habilitarlo en el driver. Propagar el indicador a las tareas de análisis aisladas; la ruta normal debe conservar su rechazo. Archivos: `ProjectCompilation.h/.cpp`, `ProjectMethodBodyModel.h`, `MethodBodyCallableLifecycle.cpp`, `ConstructionAndLifetimeExpressions.cpp`, suite y bitácora. Comprobar selección real del constructor implícito, plan de emisión y rechazo antes de generar código.
- Resultado 2026-11-05: una compilación experimental analizó `new Demo.Implicit()`, obtuvo el binding real del constructor, generó el plan y emitió un objeto cuyo símbolo definido es `??0Implicit@Demo@@QEAA@XZ`. La opción exige un proveedor y no puede habilitarse tras comenzar; una compilación independiente confirmó el bloqueo antes de layout/HIR. El primer intento encontró la barrera de métodos importados de la configuración de prueba; se habilitaron sus opciones experimentales únicamente dentro del arnés y se repitió. Reconstrucción ON, contrato estático, suite y prueba de lifetime, regresiones estática/DLL y `git diff --check`: correctos. El objeto seleccionado aún no se enlaza ni ejecuta desde NativeSharp, y la destrucción real no está conectada. El driver no activa esta opción. Cambios sin commit.

## Proveedor experimental de métodos en ProjectCompilation

- Objetivo: antes de construir los índices de miembros, obtener automáticamente las declaraciones de instancia desde las sesiones Clang y asociarlas con tipos C++ ya instalados en el lookup. Los records de definición conflictiva o múltiple permanecen fuera de esta primera ruta; los nombres con sobrecargas no proyectables o ambiguas se omiten completos para no presentar una familia parcial. Todo sigue desactivado en el driver.
- Protección: si el proveedor instala uno o más métodos, detener la compilación antes de herencia y HIR con un error explícito, ya que la identidad y el ABI de la llamada aún no están implementados. No reutilizar snapshots para miembros experimentales ni publicarlos como metadata NSMETA; con el proveedor desactivado se preserva el comportamiento actual.
- Presupuesto: `ProjectCompilation.h/.cpp`, `ProjectCompilationSnapshots.cpp`, `CppSemanticFunction.h/.cpp`, `CppClangSessionTests.cpp` y esta bitácora. Comprobar un encabezado físico, obtención sin lista manual, símbolo/receptor, propietarios, configuraciones tardías y rechazo antes de generar código. No modificar el driver.
- Evidencia 2026-11-05: `ProjectCompilation` admite un proveedor opcional antes de iniciar y lo ejecuta tras resolver los tipos; `provideExperimentalCppInstanceMethods` obtiene los métodos de las sesiones de records con definición única y solo de tipos efectivamente importados. Una familia con miembro estático, contrato no proyectable o firma indistinguible queda excluida completa. Un encabezado físico produjo automáticamente `Demo.Widget::ping(int) const` en el índice, con símbolo `?ping@Widget@Demo@@QEBAHH@Z`; `volatile` y la familia mixta estática/instancia no se publicaron. Otro proyecto de prueba confirmó que el proveedor con métodos detiene el avance antes de HIR. La ruta deshabilita restauración/captura de snapshots y cachés relevantes; esta política aún requiere pruebas específicas con módulos precompilados. Reconstrucción de NSC y suite, contrato estático, llamadas mixtas y `git diff --check`: código 0. El driver no configura este proveedor y todavía no se emiten llamadas a métodos.
- Resultado parcial: el local noexcept termina correctamente al cerrar un bloque en O0/O2; una función separada que retorna dentro del ámbito falla porque su unidad LLVM no declara el destructor C++ (la partición solo registra layouts struct nativos). Siguiente ajuste limitado: `src/LLVM/ProjectOopCodePartition.cpp` incorporará las dependencias de destructores C++ en unidades que incluyen limpieza automática; se verificará retorno anticipado, ámbitos, referencia previa y rechazos. Actualizar la prueba `referencia-local-cpp` para afirmar que el local trivial ya se admite, conservando su cobertura de referencias. Sin commit hasta validar.
- Resultado 2026-11-05: compilaciones ON/OFF de NSC correctas, sin despliegue en C:. `pruebas-proyecto/lifetime-local/ejecutar.cmd` verifica con NSLink construcción y destrucción noexcept al salir del bloque y al retornar de otra función, en O0/O2; rechaza constructor/destructor potencialmente lanzadores y copia (NS3020). La partición declara preventivamente los destructores C++ importados en las unidades con limpieza automática e incluye esas declaraciones en la identidad de caché; esto es conservador y no prueba otros casos de lifetime. Las pruebas de referencias y destructor explícito pasan; la regresión de doble excepción y DLL pasa O0/O2 con LLD. El enlace de esa frontera de excepciones mediante NSLink sigue sin resolver: NSL1023 para `_guard_dispatch_icall` al usar las bibliotecas CRT. No se afirma interoperabilidad total ni se ha hecho commit.
- Siguiente verificación dirigida: agregar a `pruebas-proyecto/lifetime-local` un caso de excepción NativeSharp que abandona el ámbito de un local C++ noexcept y comprueba desde el catch que el destructor se ejecutó; ejecutar con el driver y NSLink en O0/O2. Añadir también un caso negativo explícito de `move` para confirmar la barrera vigente. Únicamente fixtures, script y bitácora; no modificar compilador ni NSLink durante esta medición.
- Resultado 2026-11-05: el driver con NSLink ejecutó correctamente en O0/O2 la destrucción del local C++ noexcept durante el desenrollado de una excepción NativeSharp capturada; la prueba observó el estado del destructor después del catch. `move(value)` fue rechazado antes de emitir código, mediante la barrera de construcción local directa. Esto no prueba excepciones C++ atravesando el runtime ni habilita traslado de objetos.
- Siguiente verificación dirigida: repetir los casos de ámbito, retorno y excepción NativeSharp con constructor/destructor explícitos noexcept exportados desde una DLL C++ y la biblioteca de importación, mediante el driver ordinario y NSLink. Confirmar las importaciones del ejecutable y los resultados O0/O2. Archivos nuevos bajo `pruebas-proyecto/lifetime-local-dll` y esta bitácora; no cambiar el compilador ni NSLink.
- Resultado 2026-11-05: `pruebas-proyecto/lifetime-local-dll/ejecutar.cmd` compiló una DLL C++ con Clang oficial y comprobó desde proyectos NativeSharp separados el destructor al salir del ámbito, al retornar y al propagar una excepción NativeSharp hasta el catch, en O0/O2. `llvm-readobj --coff-imports` verificó para cada ejecutable la importación de `ready.dll` y los símbolos del constructor y destructor; todos devolvieron 67. Alcance limitado a los constructores y destructores noexcept de la prueba: no demuestra excepciones C++ ni interoperabilidad completa.
- Siguiente verificación dirigida: extender los fixtures del driver para observar destrucción de local C++ noexcept al salir de un ciclo mediante `break` y `continue`, con enlace estático y DLL en O0/O2. Modificar únicamente fixtures, scripts de `lifetime-local` y `lifetime-local-dll` y esta bitácora. No modificar el compilador antes de reproducir y localizar un eventual fallo.
- Resultado 2026-11-05: `break` y `continue` destruyen el local C++ noexcept correctamente según el estado observado al abandonar o continuar el ciclo. Ambos casos pasaron con biblioteca estática y DLL en O0/O2; los scripts también repitieron ámbito, retorno y excepción NativeSharp. En DLL se verificaron las importaciones del constructor y destructor.
- Siguiente reproducción dirigida: añadir a ambos fixtures un tipo C++ con constructor explícito sin argumentos y destructor noexcept, y comprobar su ciclo de vida en el driver real en O0/O2. La guarda HIR actual solo admite constructor predeterminado implícito o explícito con `int`, por lo que esta capacidad aún no se afirma. Primero cambiar solo fixtures y bitácora; investigar el resultado antes de modificar el compilador.
- Reproducción 2026-11-05: el proyecto `lifetime-default` con `Demo.Default::Default() noexcept` explícito llega al guard de HIR y es rechazado con `experimental C++ lifetime requires a supported constructor and unique destructor`. La causa observada es que el guard requiere `cppLifetimeImplicit` para cualquier constructor sin parámetros, aunque la declaración explícita, el símbolo y destructor ya fueron proyectados. Siguiente cambio limitado a `src/Frontend/ProjectCompilation.cpp`: aceptar constructor C++ seleccionado de cero parámetros y cero argumentos, preservando verificación del miembro, destructor único y requisito noexcept para locales. Mantener protegidas copias, movimientos y el resto de firmas; repetir fixture en O0/O2 y regresiones de lifetime, referencias y excepciones. Sin modificar NSLink.
- Resultado intermedio: tras ajustar HIR, el mismo fixture llega a MIR y falla con `C++ value construction requires a supported constructor`: `MirNodeMachine.cpp` repite la condición que limita el constructor sin argumentos a implícitos. Siguiente ajuste: aplicar allí la misma condición de cero parámetros/argumentos, preservando las demás validaciones. Reconstruir ON/OFF y repetir el fixture y regresiones; no afirmar éxito antes de ejecutarlas.
- Resultado parcial 2026-11-05: tras ajustar los guards de HIR y MIR, el proyecto `lifetime-default` enlazado estáticamente ejecuta el constructor explícito sin argumentos y destructor noexcept en O0/O2. Las regresiones de locales con parámetros y DLL (ámbito, retorno, excepción, break, continue, copia y move) pasan. La construcción OFF pasó; la ON completó `nsc.exe` y sigue compilando la suite. Falta verificar el constructor explícito sin argumentos desde DLL y ejecutar la suite y regresión de excepciones completas.
- Para completar la verificación inicialmente prevista de este mismo constructor explícito sin argumentos, agregar un fixture hermano `pruebas-proyecto/lifetime-default-dll` que enlace la DLL y compruebe por importaciones y ejecución el constructor y destructor en O0/O2; únicamente fixtures y bitácora.
- Resultado 2026-11-05: `lifetime-default/ejecutar.cmd` y `lifetime-default-dll/ejecutar.cmd` pasan en O0/O2 usando el driver real y NSLink; en DLL, `llvm-readobj` confirma el constructor y destructor importados desde `default.dll`. ON y OFF compilan; pasan también referencias locales, cinco salidas de ámbito con enlace estático y DLL, rechazo de copia/move y la suite de lifetime/doble excepción (con LLD para la frontera de excepciones). Durante la reconstrucción hubo una colisión temporal porque la suite anterior aún ocupaba Ninja; se esperó a que terminara y se reconstruyó correctamente, sin limpiar. El requisito de enlazar excepciones C++ completas con NSLink sigue pendiente por NSL1023. No se ha hecho commit.
- Siguiente reproducción dirigida: comprobar con el driver ordinario un constructor C++ explícito `noexcept` que recibe `int` y `double`, cuya implementación verifica ambos valores y cuyo destructor registra su ejecución. Los guards actuales solo admiten cero argumentos o un `int`; registrar el resultado antes de cambiar HIR/MIR. Archivos nuevos `pruebas-proyecto/lifetime-scalar` y esta bitácora; no cambiar compilador ni NSLink en esta medición.
- Reproducción: `lifetime-scalar` llega a HIR y falla con `experimental C++ lifetime requires a supported constructor and unique destructor`. Clang ya proyecta ambos parámetros escalares; los guards HIR y MIR solo permiten un `int`. Siguiente ajuste acotado: permitir adicionalmente el constructor seleccionado con parámetros por valor `int, double` y dos argumentos, en `ProjectCompilation.cpp` y `MirNodeMachine.cpp`, preservando el requisito noexcept y las demás protecciones. Verificar recepción de valores y destrucción con Clang/NSLink en O0/O2, regresiones existentes y compilación ON/OFF. Es una ampliación parcial, no cobertura de todos los escalares.
- Resultado parcial: `lifetime-scalar` pasó O0/O2 con NSLink; el constructor C++ observó exactamente `42` y `2.5`, y el destructor se ejecutó. La configuración OFF compila; la ON terminó `nsc` y aún construye la suite. Pasan regresiones de locales y DLL. La condición actual `int,double` es solamente un paso de diagnóstico y no representa la solución general de constructores escalares.
- Siguiente reproducción: ampliar `lifetime-scalar` con otro tipo cuyo constructor recibe seis escalares mezclados (`int`, `double`, `float`, `int`, `int`, `double`) y verifica cada valor, de manera que Win64 utiliza registros y pila. Añadir fixture y extensión del script; comprobar primero el rechazo actual, sin cambiar el compilador. Esto prepara el soporte general de parámetros escalares proyectados y no supone que una sola firma pruebe todas las combinaciones.
- La firma de seis escalares fue proyectada y alcanzó el mismo guard HIR, que la rechazó; el caso `int,double` anterior sigue funcionando. `projectCppSemanticLifetimeMethod` solo proyecta valores de tipos escalares reconocidos por `mapCppScalarType`; el backend ya baja argumentos y construye la firma a partir de esos tipos. Siguiente implementación: reemplazar en ambos guards las firmas enumeradas por validación de cantidad y de cada parámetro por valor dentro del conjunto escalar que proyecta Clang. Preservar los miembros y destructor verificados, noexcept para locales, y rechazo de referencias, agregados, copia y movimiento. Probar los seis argumentos en O0/O2 y mantener las otras regresiones; no afirmar cobertura exhaustiva de combinaciones solo por este test.
- Resultado parcial 2026-11-05: los constructores `Scalar(int,double)` y `Wide(int,double,float,int,int,double)` y sus destructores pasaron con NSLink en O0/O2; el código C++ verificó cada argumento. También pasaron los casos anteriores de ciclo de vida estático y DLL. OFF compiló; ON enlazó `nsc` y sigue construyendo la suite. El guard ahora admite los escalares proyectados, pero los demás tipos y combinaciones no están verificados exhaustivamente.
- Siguiente verificación: compilar las mismas dos firmas como DLL con Clang, comprobar las importaciones y ejecutar ambos casos en O0/O2 desde el driver real. Crear `pruebas-proyecto/lifetime-scalar-dll` y registrar resultado; no modificar compilador ni NSLink.
- Resultado intermedio DLL: `lld-link /nodefaultlib` falla por `_fltused` al compilar las comparaciones de punto flotante; `llvm-nm --defined-only` confirma que `NativeSharp.Runtime.Win32.lib` define ese símbolo. Siguiente verificación limitada: suministrar esa biblioteca estática al enlazador de la DLL de prueba, inspeccionar dependencias y ejecutar la prueba; no inferir que otros componentes CRT estén resueltos.
- La biblioteca de NativeSharp agrupa `_fltused` con el runtime completo en un solo objeto, por lo que al extraerlo aparecen numerosas dependencias Win32 y `__chkstk`; no es una dependencia mínima para este fixture. `llvm-nm` confirma también `_fltused` definido en `A:/vc2026-runtime/lib/x64/release/libcmt.lib`. Siguiente comprobación: enlazar esa biblioteca estática y comprobar las importaciones resultantes de la DLL.
- Resultado 2026-11-05: la validación de constructor pasó de firmas enumeradas a parámetros escalares por valor proyectados por Clang, preservando los demás guards. `lifetime-scalar/ejecutar.cmd` y `lifetime-scalar-dll/ejecutar.cmd` pasan en O0/O2 para dos y seis argumentos mixtos; el código C++ verifica valores y destrucción y las pruebas DLL comprueban imports de constructor y destructor. La DLL de prueba resolvió `_fltused` desde `libcmt.lib` estática; `llvm-readobj --coff-imports` no muestra importaciones en esa DLL. Construcciones ON/OFF y regresiones de locales, DLL y doble excepción pasan. No se han verificado exhaustivamente todos los escalares, firmas o combinaciones; la frontera de excepciones C++ con NSLink permanece pendiente. Sin commit.
- Siguiente verificación: extender los fixtures `lifetime-scalar` y `lifetime-scalar-dll` con un constructor `noexcept` que reciba `bool`, enteros con y sin signo de 8, 16, 32 y 64 bits; validar valores negativos y grandes, destrucción y enlace estático/DLL en O0/O2. Crear fixture NativeSharp con variables tipadas, y comprobar símbolos importados en DLL. Modificar solo fixtures/scripts y esta bitácora; cualquier problema del compilador se reproduce antes de corregirlo. No afirmar cobertura completa del ABI por una muestra.
- Resultado 2026-11-05: `Narrow(bool, signed char, unsigned char, short, unsigned short, unsigned int, long long, unsigned long long)` verificó valores negativos, altos sin signo y destrucción en O0/O2 con bibliotecas estática y DLL; las importaciones de constructor/destructor se inspeccionaron en DLL. También pasaron las firmas previas. Esto no demuestra todas las permutaciones ni punteros/referencias.
- Siguiente reproducción dirigida: en `lifetime-scalar` añadir constructor C++ explícito `noexcept` que recibe `int*`, escribe mediante ese puntero y se destruye al salir del ámbito; NativeSharp observará tanto la escritura como el destructor. Comprobar el resultado en O0/O2 con Clang y NSLink antes de modificar la proyección; limitar cambios a fixture, script y bitácora. Los punteros son prestados por esta prueba y no cambian ownership.
- Reproducción 2026-11-05: `Pointer(int*)` falla en el driver con `struct has only an implicit parameterless constructor`; `projectCppSemanticLifetimeMethod` descarta parámetros puntero al usar `mapCppScalarType`, mientras `mapFunctionType` ya proyecta punteros primitivos no const con tamaño x64 y excluye referencias y const no representado. Siguiente ajuste: reutilizar esa proyección en parámetros de lifetime y admitir en HIR/MIR únicamente punteros por valor a tipos primitivos proyectados, conservando el rechazo de referencias, records por valor y const no representado. Archivos `CppSemanticFunction.cpp`, `ProjectCompilation.cpp`, `MirNodeMachine.cpp`, fixtures estático y DLL y bitácora; probar escritura mediante puntero prestado y destrucción en O0/O2, inspeccionar imports, ejecutar regresiones y builds ON/OFF. No modificar NSLink.
- Resultado intermedio: al proyectar `int*`, el proyecto completo falla antes de HIR con `invalid C++ lifetime parameter type`; la validación `ProjectMemberModel::setExperimentalCppLifetimeMethods` solo acepta `experimentalCppScalarBytes` y debe reconocer los mismos punteros primitivos proyectados. Siguiente corrección limitada a esa validación en `ProjectMemberModel.cpp`: quitar sufijos de puntero para verificar base escalar o void y conservar rechazo de valores no soportados; recompilar y repetir pruebas. La ejecución anterior `Pointer-O0.log` es anterior al build actual; el error vigente está en `Main-O0.log`.
- Regresión observada: `nsc_cpp_session_tests` falla `project lifetime family retains explicit, implicit, deferred and colliding overloads` porque el único constructor diferido del fixture era `Widget(int*)`, ahora proyectado. Siguiente ajuste de prueba en `tests/unit/CppClangSessionTests.cpp`: verificar explícitamente la proyección `int*` y añadir `Widget(int&)` como ejemplo distinto aún diferido, preservando las verificaciones previas de colisión y métodos implícitos; recompilar y ejecutar la suite. No suprimir el caso negativo ni asumir soporte de referencias.
- Resultado 2026-11-05: se reutilizó la proyección de parámetros para punteros primitivos no const y se adecuaron las validaciones de miembros, HIR y MIR. El constructor `Pointer(int*)` escribió `42` en memoria reservada por NativeSharp y su destructor se ejecutó correctamente con enlace estático y DLL en O0/O2; las importaciones de DLL se verificaron. Las firmas escalares anteriores también pasan. ON/OFF compilan; la suite completa de lifetime y doble excepción pasa después de actualizar la prueba antigua: `Widget(int*)` se comprueba como proyectado y `Widget(int&)` se mantiene como caso diferido. No se ha probado exhaustivamente const, punteros múltiples o a records, ni ownership o excepciones C++ con NSLink. Sin commit ni despliegue en C:.
- Siguiente reproducción: extender `lifetime-scalar` con un constructor C++ único que recibe `const int*`, lee el valor y destruye el objeto; llamar desde NativeSharp con un puntero mutable válido y observar el resultado. El mapeo actual de lifetime rechaza pointee const y el modelo `CppImportedLifetimeMethod` no conserva su calificación. La evaluación de candidatos de constructor sí consulta `pointerPointeeReadonly`. Cambiar solo fixtures y script antes de reproducir; no afirmar selección de sobrecargas const/mutable.
- Reproducción: `ReadPointer(const int*)` se descarta y el driver informa `struct has only an implicit parameterless constructor`. `mapFunctionType(..., true)` conserva `isConst` para pointee y `ConstructorCandidateEvaluation.cpp` comprueba `pointerPointeeReadonly`, pero `CppImportedLifetimeMethod` no transporta esa información. Siguiente cambio: permitir ese mapeo, transportar por parámetro la calificación a `MethodParameterSymbol`, validar longitud y preservar como ambiguas/diferidas las familias que colisionan solo por const. Archivos `CppImportedInstanceMethod.h`, `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`, fixtures y bitácora. Verificar puntero mutable a constructor const, puntero readonly a const y rechazo de readonly a constructor mutable, además de DLL y regresiones. No afirmar selección de sobrecargas const/mutable.
- Resultado intermedio: `ReadPointer(const int*)` pasó con puntero mutable y con `int const*` proveniente de método C++ estático, con enlace estático y DLL en O0/O2. `Pointer(int*)` rechazó el argumento `int const*` con `no viable constructor accepts the argument types`. En el fixture se corrigió el nombre reservado `get` y la declaración que descartaba `const`; ambos eran errores del fixture, no del compilador. Siguiente verificación negativa: agregar clase con sobrecargas `int*` y `const int*` y comprobar que no se elija silenciosamente una mientras la distinción de sobrecarga permanece pendiente. Solo fixtures y bitácora.
- Resultado 2026-11-05: `const int*` en constructor único se proyecta conservando `parameterPointeeReadonly` hasta `MethodParameterSymbol`. Con biblioteca estática y DLL y en O0/O2 se ejecutaron tanto puntero mutable a constructor const como puntero readonly a constructor const; los valores y destrucción fueron comprobados. Puntero readonly a constructor mutable se rechaza con `no viable constructor accepts the argument types`. Las regresiones de locales y la suite de lifetime/doble excepción pasan; ON/OFF compilan. `Choice(int*)`/`Choice(const int*)` se rechaza sin seleccionar una sobrecarga, pero emite el diagnóstico equívoco `struct has only an implicit parameterless constructor`; selección y diagnóstico correctos quedan pendientes. No se han probado todas las combinaciones const ni cambiado NSLink. Sin commit ni despliegue en C:.
- Análisis de sobrecargas: la familia `Choice(int*)`/`Choice(const int*)` se descarta porque `collectCppSemanticLifetimeFamilies` usa solo `nativeSharpTypeKey` como firma; después, `setExperimentalCppLifetimeMethods` vuelve a deduplicar por la misma clave y `betterCandidate` no prefiere el parámetro mutable cuando ambos son viables. `ConstructorCandidateEvaluation` ya descarta quitar const. Siguiente implementación limitada a sobrecargas C++ de un solo puntero primitivo: distinguir `const` en firmas proyectadas y del miembro, mantener colisiones de otros casos, y aplicar el desempate de coincidencias exactas favoreciendo `int*` sobre `const int*` cuando ambos candidatos son viables. Archivos `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`, `ConstructorCandidateEvaluation.cpp`, fixtures y bitácora. Comprobar contra Clang selección mutable/readonly, estático/DLL O0/O2 y regresiones; no habilitar otras sobrecargas no resueltas ni modificar NSLink.
- Resultado parcial: `Choice(int*)` y `Choice(const int*)` compilaron y ejecutaron la sobrecarga esperada para puntero mutable y readonly con enlace estático/DLL en O0/O2, verificando símbolos importados; un control C++ compilado con Clang observó las mismas selecciones. Antes de cerrar esta comprobación, agregar una llamada con `null` para verificar que no se elija arbitrariamente entre dos candidatos C++ viables; fixture y script solamente. Las demás combinaciones y referencias siguen pendientes.
- Resultado 2026-11-05: para las sobrecargas C++ de un solo puntero primitivo que difieren en `const`, el driver seleccionó `int*` para puntero mutable y `const int*` para puntero readonly, coincidiendo con el control Clang; O0/O2 pasaron con enlace estático y DLL y se inspeccionaron sus símbolos importados. La llamada con `null` se rechazó con `ambiguous constructor invocation`. Las configuraciones ON/OFF compilaron y la regresión de lifetime/doble excepción pasó. Durante una interrupción de herramientas se perdieron los identificadores de ejecución; se comprobó ausencia de `nsc.exe` y del resultado de `null` antes de repetir la prueba. No se afirman otras combinaciones de sobrecargas. Sin commit ni despliegue en C:.
- Tanda siguiente: ampliar selección de sobrecargas de constructores C++ con varios punteros primitivos mutable/const. Primero reproducir dos familias: `Pair(int*,int*)`/`Pair(const int*,const int*)`, y `Cross(int*,const int*)`/`Cross(const int*,int*)`; verificar selección para argumentos mixtos y ambigüedad cuando cada candidato es mejor en un argumento. Comparar con Clang y repetir estático/DLL en O0/O2. Empezar con fixtures y bitácora; después de reproducir, ajustar únicamente proyección, identidad y comparación de candidatos con pruebas existentes preservadas. No habilitar referencias, templates ni otras capacidades no verificadas. Agrupar las ediciones y ejecutar regresión amplia al terminar la tanda.
- Reproducción: `Pair(int*,int*)`/`Pair(const int*,const int*)` se descarta antes de resolver la llamada con el diagnóstico de constructor implícito. Las claves de familia, validación de miembros y firmas de miembro solo incorporan `const` con un parámetro; `betterCandidate` solo desempata ese caso. Cambiar esos tres lugares para conservar la calificación por parámetro y comparar, para candidatos C++ viables y conversiones exactas equivalentes, cada argumento: si un candidato empeora por const en cualquiera no domina; si mejora en alguno y no empeora, domina. Mantener otros tipos/colisiones bajo las reglas existentes. Verificar ambas familias, ambigüedad cruzada/null, control Clang, DLL, O0/O2 y regresiones agrupadas.
- Reorientación expresa del usuario 2026-11-05: completar las tandas del tracking en orden, implementando sus puntos como conjunto y validando al cierre; no reconstruir todo ni ejecutar batería integral por cada elemento. Se actualizó `A:/LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md` §14. Primera tanda: consolidar los puntos T01-007, 008 y 011–013 en `LINEA-BASE-TANDAS.md` y `TRACKING.md` a partir de rutas, caché y reglas observadas; no alterar código, configuración, compiladores o pruebas para este cierre documental. La evidencia histórica de 2026-11-03 se conserva separada de la observación actual. La edición previa de sobrecargas de varios parámetros permanece en curso y sin validar; no declararla terminada al cerrar la tanda 1.
- Tarea persistente reiniciada por el usuario con lineamientos actualizados: completar las 18 tandas en orden, trabajar cada tanda como conjunto y reservar su validación al cierre, sin reconstrucción integral ni batería completa por elemento. Tanda 01: 13 puntos documentados y cotejados con fuentes; la línea base histórica se conserva. Tanda 02 en curso: revisar en conjunto T02-005/006/011/014/016–020, confirmar versión y rutas reales, target, runtime, linker, dependencias y opciones efectivas; agregar únicamente la prueba faltante si la evidencia existente no cubre compilación normal sin sesiones, y documentar reproducción. Validar el conjunto una sola vez al terminar; no tocar NSLink ni desplegar en C:. El trabajo previo de sobrecargas de varios parámetros queda preservado y aún no validado.
- Cierre de la tanda 02, 2026-11-05: los 20 puntos están marcados con referencia a [TANDA-02-CONSTRUCCION.md](TANDA-02-CONSTRUCCION.md) o evidencia previa. La validación agrupada compiló incrementalmente NSC ON/OFF y ejecutó `nsc_cpp_session_tests` con código 0, incluida la nueva fábrica que detecta aperturas inesperadas para proyecto sin referencias. Se inspeccionaron imports del compilador ON. No se ejecutó una reconstrucción integral por elemento. La implementación previa de sobrecargas de varios parámetros permanece sin validar y no se considera cerrada por esta tanda de construcción.
- Tanda 03 en curso: auditar conjuntamente los 22 puntos del contrato público de sesión. Los tipos de opciones, diagnósticos, apertura, cierre y cancelación ya existen; falta una identidad reproducible explícita de configuración y documentar concurrencia. Implementar una identidad SHA-256 versionada sobre campos propios de `CppSessionOptions`, con secuencia y longitudes inequívocas, e incluir contenido del buffer cuando corresponda. Es identidad de configuración, no clave completa de caché ni prueba del contenido transitivo de archivos. Documentar propiedad, cierre y concurrencia sin prometer lo no demostrado; agregar pruebas de estabilidad y sensibilidad de todos los campos y del orden. Archivos previstos: `CppSessionOptions.h`, implementación nueva en `src/Interop/Cpp`, `CMakeLists.txt`, `CppInteropSession.h`, `CppClangSessionTests.cpp`, documento de tanda y tracking. Implementar todo el conjunto antes del build y validación de la tanda, sin reconstrucción integral por elemento.
- Primera construcción agrupada de la tanda 03: ON/OFF compilaron, pero la revisión identificó que normalizar `..` en una ruta de configuración puede confundir rutas distintas cuando hay enlaces simbólicos. Además, Clang advirtió inicializadores agregados incompletos para el campo nuevo `parameterPointeeReadonly`. Siguiente corrección conjunta: conservar el texto genérico UTF-8 de la ruta sin normalización léxica, probar rutas distintas y completar los inicializadores explícitamente. Solo recompilar los archivos afectados de ON y ejecutar la suite de la tanda; OFF no consume la identidad y ya pasó.
- Auditoría del cierre de la tanda 03: T03-014 exige expresamente el resultado y fallo de apertura en `CppInteropSession.h`, pero están en `CppInteropSessionFactory.h`. La comprobación anterior omitió esa diferencia; se reabrió T03-014 y la tanda 03. Siguiente corrección agrupada del contrato: trasladar las declaraciones sin cambiar sus tipos o namespace, preservar la inclusión desde la fábrica y agregar aserciones de compilación usando únicamente el header de sesión. Verificar de forma dirigida sin reconstrucción general para este cambio de ubicación. Archivos de ambos headers, fixture de interfaz y documento de tanda.
- Corrección de T03-014: resultado y fallo de apertura trasladados a `CppInteropSession.h`, conservando los mismos tipos y namespace. El fixture los comprueba antes de incluir la fábrica; `verificar-interfaces-tanda1.cmd` pasó con `/W4 /WX`. Con los demás puntos y la validación agrupada registrados en `TANDA-03-CONTRATO.md`, queda cerrada la tanda 03. No se hizo una reconstrucción general para este ajuste de encabezado.
- Tanda 04 en análisis: la sesión actual construye argumentos y colector dentro de `ClangSession.cpp` y utiliza `clang::tooling::buildASTFromCodeWithArgs`; no existen los archivos separados propuestos `ClangInvocationBuilder.cpp`, `ClangDiagnosticConsumer.cpp` ni `ClangSessionAction.cpp`. En Clang 23.1.0, `clang/lib/Tooling/Tooling.cpp` crea `ASTBuilderAction`, `ToolInvocation` y diagnósticos con consumidor no poseído; `ASTUnit.cpp` crea la acción y administra `BeginSourceFile`/`EndSourceFile`. La sesión solo consulta cancelación antes y después del análisis, no durante. Ninguno de estos hallazgos cierra automáticamente los ítems que exigen archivos o comportamientos concretos. Continuar trazando el ciclo completo y diseñar la implementación agrupada de T04-001–030 antes de construir o probar la tanda.

### Tanda 04: implementación agrupada

- Alcance: separar la construcción de argumentos y el consumidor de diagnósticos;
  preparar la acción de sesión y la cancelación en los puntos de E/S que Clang
  permite interceptar, preservando la propiedad y el ciclo de vida del AST.
- Escritura: `A:/nsc-devel/src/Interop/Cpp`, encabezados privados de esa carpeta,
  `A:/nsc-devel/CMakeLists.txt` y `A:/nsc-devel/tests/unit/CppClangSessionTests.cpp`;
  documentación y evidencia en `A:/llvm-project-al82dev/nativesharp-interop`.
- No se modifica el código de LLVM/Clang ni NSLink; no se despliega en C:.
- Los cambios se registrarán en commits separados. La construcción incremental y
  las pruebas dirigidas se agruparán al final de la tanda; sus resultados no se
  anticipan. La cancelación por E/S no promete interrumpir instantáneamente CPU.
- Implementación agrupada de tanda 04: extraer el constructor de argumentos y el consumidor de diagnósticos privados a los archivos previstos; implementar `ClangSessionAction.cpp` como `clang::tooling::ToolAction` propia sobre `ASTUnit::LoadFromCompilerInvocation`, reproduciendo el flujo oficial 23.1.0 de `Tooling.cpp` con `CompilerInstance` y acción administrados por ASTUnit. Conservar VFS por sesión y propiedad no transferida del consumidor. Añadir VFS privado que compruebe el token en operaciones de lectura/consulta, además de comprobaciones antes y después del análisis; esto no promete interrupción inmediata de trabajo puramente de CPU. Probar construcción de opciones, rutas, diagnósticos/notas, propiedad, apertura/cierre y cancelación en puntos I/O, y regresiones de sesiones independientes al cierre. Archivos de `src/Interop/Cpp`, headers privados, `CMakeLists.txt`, suite `CppClangSessionTests.cpp` y documentación; no modificar LLVM/Clang ni NSLink. Ejecutar construcción y validación una vez tras completar la tanda, agrupar errores si aparecen.
## Tanda 08: indirección adicional sobre typedef de callback C

- Objetivo: permitir que NativeSharp use como puntero `Callback*` un typedef C de puntero a función con prototipo y pase ese valor a una función C que recibe `Callback*`.
- Evidencia: `QualifiedTypeNames.cpp` rechaza todo modificador en el sitio de uso del alias; `ProjectMemberModel::canonicalTypeKey` agrega modificadores después de la identidad estructural de función y `CppSemanticFunction.cpp::mapFunctionType` recorre punteros adicionales. Estas rutas deben comprobarse juntas en un proyecto ejecutable.
- Presupuesto: `src/Frontend/NameLookup/QualifiedTypeNames.cpp`, los archivos `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-callback`, los registros Markdown de la tanda y del commit. Permitir únicamente modificadores de puntero; conservar los rechazos de otros modificadores y de calificadores aún no representados.
- Preservar la ruta original `Callback`, la convención y firma importadas, las demás rutas del compilador y el árbol LLVM/Clang. No modificar NSLink ni desplegar en C:. La validación se agrupará con la de la tanda 08 y este caso aislado no cierra T08-017.

## Tanda 08: prueba de matriz C incompleta

- Objetivo: comprobar que un typedef C de matriz `[][3]` conserva su límite exterior incompleto al consultar `sizeof` del puntero y del propio tipo.
- Presupuesto: `Api.h`, `Pointer.ns.txt`, `SizeofMatrix.ns.txt` y `validar.cmd` de `nativesharp-interop/pruebas-tanda-08-incompleto`, más registro Markdown. No modificar producción por anticipado; si la validación detecta un defecto se investigará y delimitará su corrección.
- Verificación prevista: construcción incremental y batería agrupada de la tanda. El caso no cierra T08-013 ni acredita todas las dimensiones y usos.

## Tanda 08: puntero a callback con firma enum

- Objetivo: comprobar la segunda ruta de expansión del alias `ColorCallback*`, cuya firma incluye un enum importado; conservar firma, convención C y modificador de puntero adicional.
- Presupuesto: `tests/unit/CppClangSessionTests.cpp` y registro Markdown. Esta prueba semántica complementa la ejecución C/NativeSharp del caso escalar; no sustituye una llamada ejecutable con enum ni cierra T08-017.
- Verificación: junto a las demás pruebas de la tanda, sin construir la suite por cada cambio.

## Tanda 08: escalares flotantes C en llamada ejecutable

- Objetivo: verificar que parámetros y resultados C `float` y `double`, también por typedef, se importan y usan desde NativeSharp con valores exactamente representables.
- Presupuesto: `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-callback`, más registro Markdown. No presupone soporte de `long double` ni otras plataformas.
- Verificación: la biblioteca C compilada por Clang y el ejecutable NativeSharp del proyecto deben producir los resultados esperados en la batería agrupada. No cerrar T08-007 con esta muestra.

## Tanda 08: callback C con parámetro y resultado enum

- Objetivo: comprobar la ejecución de un callback NativeSharp cuya firma recibe y devuelve un enum C y que es invocado por una función C.
- Presupuesto: `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-enum-funcion`, más registro Markdown. No modificar el compilador antes de observar el resultado ni cambiar contratos ajenos.
- Verificación: construcción incremental y batería agrupada de la tanda; un resultado correcto cubre este fixture de Windows x64 O0 y no cierra T08-009 ni T08-017.

## Tanda 08: campos C de enum en ejecución

- Objetivo: comprobar acceso de NativeSharp a un campo enum C y a un elemento de arreglo de enum C dentro de un struct, verificado por una función C.
- Presupuesto: `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-enum-funcion`, más registro Markdown. Se usa un puntero a struct; no se habilita paso de agregados por valor.
- Verificación: ejecutar el proyecto dentro de la batería agrupada y conservar cualquier fallo. El caso no cubre todas las representaciones de enum o arreglos.

## Tanda 08: enteros C con signo y sin signo

- Objetivo: comprobar llamadas C con aliases de enteros de 8, 16 y 32 bits, incluyendo un argumento negativo y valores positivos de tipos sin signo.
- Presupuesto: `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-callback`, y registro Markdown. No cambiar el compilador antes de obtener evidencia del fixture.
- Verificación: proyecto mixto con Clang 23.1.0 y NativeSharp en la batería agrupada. Esta muestra no cierra T08-006 ni cubre todos los valores o tipos de C.

## Tanda 08: global C de tipo enum

- Objetivo: consumir una variable global externa C de tipo enum conservando su identidad nominal, tamaño, signo y calificadores, y seleccionar el enum cuando sea requerido por el global aunque no aparezca explícitamente en código NativeSharp.
- Evidencia inicial: `projectCppSemanticGlobal` llama `mapGlobalType`, que solo proyecta escalares y punteros a escalares; `projectCppSemanticEnumType` valida enum contra el índice de tipos de `ProjectNameLookup`. El proveedor de globales no recibe actualmente ese índice; la observación previa de dependencias contempla firmas de función, pero no tipos de globales.
- Presupuesto inicial de reproducción: `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-enum-funcion`, y registro Markdown. Para la implementación se delimitarán símbolos y consumidores tras completar la inspección del orden de selección; no modificar NSLink, LLVM ni desplegar en C:.
- La prueba mixta inicial usa un enum también mencionado por funciones, por lo que no demuestra la selección de dependencias cuando solo aparece en el global. Añadir un caso independiente para esa condición antes de considerar resuelto el contrato.

### Proyección inicial con índice de tipos

- Presupuesto: `include/nativesharp/Interop/CppSemanticInventory.h`, `src/Interop/Cpp/CppSemanticInventory.cpp`, `src/Driver/PrecompiledModuleConfiguration.cpp`, el registro Markdown y la prueba mixta ya preparada. El proveedor habitual consultará el `ProjectNameLookup` ya construido; se mantiene la API anterior para sus otros consumidores.
- Proyectar globales enum solo cuando `projectCppSemanticEnumType` confirme identidad, propietario, tamaño, alineación y tipo base. Preservar selección de nombres, detección de conflictos, símbolo, const y los rechazos actuales de TLS y calificaciones no representadas. No inventar identidades cuando el tipo no esté seleccionado.
- Este cambio es una parte del contrato: la selección del enum usado únicamente por un global requiere una modificación posterior en la observación previa y su propia prueba. La validación agrupada sigue pendiente; ninguna casilla se cierra por la proyección inicial.

### Selección por demanda de enum de global

- Presupuesto: `src/Interop/Cpp/CppSemanticRecord.cpp`, el proyecto `pruebas-tanda-08-enum-funcion` y registro Markdown. Reutilizar demandas, visibilidad por proyecto y namespaces, candidatos enum y controles existentes de conflicto; no agregar tipos para todos los globales sin demanda.
- Prueba: una variable C de un enum diferente, cuyo nombre de tipo no aparece en NativeSharp ni en firmas importadas, se lee como entero. Una función C de resultado `int` confirma el valor. La proyección de punteros y las colisiones por sombra requieren pruebas adicionales.
- La selección y la proyección inicial se validarán en la batería agrupada. Mantener pendientes T08-004 y T08-009 hasta cubrir los contratos restantes.

### Global enum frente a sombra local

- Objetivo: verificar que una variable local o parámetro NativeSharp llamado igual que un global C no exige resolver el enum C de ese global cuando existe un tipo NativeSharp homónimo al enum.
- Presupuesto: `Shadow.ns.txt` y `validar.cmd` de `nativesharp-interop/pruebas-tanda-08-enum-funcion`, más registro Markdown. Si falla, identificar primero la resolución causante antes de modificar producción.
- Verificación junto con la batería agrupada; este fixture no agota las formas de sombra o colisión.

### Corrección de selección especulativa de enum

- Fallo observado el 2026-11-05: el proyecto `Shadow.ns.txt` termina con `C enum global type conflicts with another type: HiddenState` aunque `hidden_global` sea un local o parámetro NativeSharp.
- Causa: la observación captura demandas sintácticas antes de la resolución de variables locales; la selección nueva transforma un candidato conflictivo en error inmediato. Debe dejar esos candidatos sin seleccionar para que la resolución posterior aplique la precedencia y, si realmente se usa el global, rechace su proyección no disponible.
- Presupuesto: únicamente `src/Interop/Cpp/CppSemanticRecord.cpp` y registro Markdown. Conservar la selección de candidatos sin conflicto y los demás diagnósticos existentes. Repetir la batería agrupada con los tres casos preparados.

### Uso real de global enum conflictivo

- Presupuesto: `Collision.ns.txt`, `validar.cmd` de `nativesharp-interop/pruebas-tanda-08-enum-funcion` y registro Markdown. Comprobar que un global C efectivamente utilizado se rechaza si su enum colisiona con un struct NativeSharp, incluso tras diferir la observación especulativa.
- El diagnóstico debe identificar que el global C no se puede proyectar; no basta con cualquier error de compilación.

### Enum requerido solo por puntero global

- Objetivo: seleccionar por demanda el enum C final de una cadena de punteros global, sin alterar la proyección ni el tratamiento diferido de colisiones.
- Presupuesto: `src/Interop/Cpp/CppSemanticRecord.cpp`, `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-enum-funcion`, más registro Markdown. Limitar el recorrido a punteros y 64 niveles; no añadir soporte para calificadores o arreglos.
- Verificar puntero y doble puntero cuyos enums no se mencionan en las firmas usadas ni en NativeSharp, además de los casos de global escalar y sombra. Ejecutar con la batería agrupada.

### Global C volatile enum

- Objetivo: proyectar un global enum con `volatile` de nivel superior, verificando identidad y layout sin perder el calificador en los accesos emitidos.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, `Api.h`, `Api.c`, `Main.ns` y `validar.cmd` del proyecto enum, y registro Markdown. No cambiar el tratamiento de punteros a volatile, campos, TLS ni otros consumidores del helper de enum.
- Comprobar ejecución y presencia de `load volatile` y `store volatile` del símbolo específico en LLVM IR, junto con la batería agrupada.

### Arreglos globales C de escalares

- Objetivo: importar globales C con arreglos fijos no calificados de elementos escalares representables, conservando dimensiones y símbolo externo para acceso indexado.
- Evidencia: `mapGlobalType` solo mapea escalares y punteros; `cppAliasArrayType` valida dimensiones fijas y `mapLlvmType` representa arreglos fijos inline. La semántica completa del acceso global todavía debe verificarse en un proyecto mixto.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, el proyecto `pruebas-tanda-08-record-c` y registros Markdown. Mantener rechazo de límites desconocidos y calificadores no representados; no modificar NSLink ni el árbol LLVM.
- Verificar escritura y lectura de un arreglo de una dimensión y una matriz mediante NativeSharp y una función C; compilar y validar agrupadamente. No cerrar T08-004 ni T08-016 por estos casos.

### No reasignación de arreglo C

- Objetivo: comprobar que un arreglo global C no se pueda reasignar como una variable escalar, aunque sus elementos sí sean modificables.
- Presupuesto: `ArrayAssign.ns.txt` y `validar.cmd` del proyecto `pruebas-tanda-08-record-c`, más registro Markdown. Si el caso compila o produce un diagnóstico ajeno, investigar la causa antes de modificar producción.
- Validar junto al acceso indexado y a la batería agrupada. No usar un fallo arbitrario como prueba del contrato.

### Corrección de reasignación de global C array

- Fallo observado: en la pasada agrupada del 2026-11-05 `shared_array = shared_array` produjo un ejecutable; el caso debía rechazarse. El proyecto de acceso indexado se compiló y ejecutó antes de esa comprobación.
- Causa localizada: `IdentifierAnalyzer.cpp` marca asignable todo estado C externo mutable sin excluir arreglos fijos, aunque el indexador ya distingue la asignabilidad de los elementos de la del arreglo completo.
- Presupuesto: `src/Frontend/Bodies/IdentifierAnalyzer.cpp` y registro Markdown. Solo cambiar la asignabilidad del estado C/C++ externo con identidad de arreglo fijo; preservar dirección, lectura, mutabilidad de elementos y estados NativeSharp.
- Verificar el rechazo específico y repetir la ejecución del acceso indexado y la batería agrupada.

### Arreglos globales C de enum

- Objetivo: importar arreglos globales C de enum sin calificar y límites fijos, seleccionando por demanda su enum nominal aunque no se use en firmas ni en el código fuente NativeSharp.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`, `Api.h`, `Api.c`, `Main.ns` de `pruebas-tanda-08-enum-funcion` y registros Markdown. Preservar límites de arreglos incompletos y rechazo de calificadores no representados; no alterar NSLink ni LLVM.
- Validación agrupada: escritura y lectura de elemento de enum mediante NativeSharp, comprobación por C y regresiones de arreglos escalares, punteros, conflictos y `const`. No cierra T08-004, T08-009 ni T08-016.

### Arreglos globales C const de escalares y enum

- Objetivo: proyectar arreglos globales fijos C cuyos elementos escalares o enum son `const`, permitir su lectura y rechazar la escritura de elementos conservando la mutabilidad de otros arreglos.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, el proyecto `pruebas-tanda-08-record-c`, el proyecto `pruebas-tanda-08-enum-funcion` y registros Markdown. La ruta aplica a elementos escalares o enum sin punteros; sigue rechazando `volatile`, límites desconocidos y calificadores no representados.
- Conservar `const` aunque Clang lo sitúe en el arreglo o en su elemento, y propagarlo como `isCppReadonly`; verificar escritura rechazada y lectura compartida con C. No cambiar otras rutas de identidad o NSLink.
- Construir incrementalmente y validar de manera agrupada todos los casos de la tanda; no cerrar T08-015 con estas muestras.

### Dirección de elemento de arreglo C const

- Objetivo: comprobar que no se obtiene un puntero mutable a un elemento `const` importado, aun cuando la escritura directa se rechace.
- Presupuesto: `ConstArrayAddress.ns.txt` y `validar.cmd` del proyecto `pruebas-tanda-08-record-c`, más registro Markdown. No cambiar producción antes de observar el caso.
- La compilación debe rechazar la conversión o dirección por motivos de inmutabilidad; validar junto con las demás pruebas.

### Arreglos globales C volatile de escalares y enum

- Objetivo: conservar `volatile` de arreglos globales C fijos hasta cada acceso indexado y emitir cargas y escrituras LLVM volatile, incluyendo matrices.
- Evidencia: el estado externo conserva `isVolatile`, pero `finishArrayRawIndex` crea direcciones sin propagarlo y `PointerLoad` genera una carga ordinaria. `storeFromParts` ya utiliza `volatileAccess` de la dirección; MIR copia ese indicador.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp`, `src/LLVM/Lowering/OopLlvmExpressionMachine.cpp`, proyectos de prueba de record y enum y registros Markdown. Solo propagar el indicador en indexación de arreglo fijo, no desde un puntero volatile hacia su apuntado.
- Verificar acceso compartido y operaciones LLVM `load volatile` y `store volatile` para arreglo y matriz, además de regresiones con arreglos mutables y const. No declarar T08-015 completo; TLS y calificadores de apuntado quedan pendientes.

### Dirección de elemento de arreglo C volatile

- Objetivo: verificar que `&volatile_array[1]` no se convierta silenciosamente en `int*` perdiendo `volatile`.
- Presupuesto: `VolatileArrayAddress.ns.txt` y `validar.cmd` del proyecto `pruebas-tanda-08-record-c`, más registro Markdown. Si se acepta, investigar y corregir la pérdida del calificador antes de considerar seguro el acceso.
- La prueba debe diagnosticar el descarte de volatile o de la dirección calificada. La validación dirigida del fixture ya compilado puede utilizarse para localizar el defecto; la batería se repetirá agrupada tras las correcciones.
- Evidencia: el compilador aceptó `&volatile_array[1]` como `int*`. La dirección indexada no conserva un tipo de puntero calificado.
- Corrección: `src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp` debe rechazar esa formación de dirección sobre arreglos globales C `volatile` de escalares o enum, incluida la indexación anidada; conservar el sombreado y los punteros no calificados. Extender el fixture con matriz y un caso positivo de arreglo mutable; registrar cada cambio con su Markdown y validar en conjunto.

### Globales C de struct

- Objetivo: importar globales C con tipo struct, puntero a struct y arreglo fijo de structs, preservando identidad, layout, campos y almacenamiento externo. Seleccionar el struct también cuando aparece únicamente en el tipo del global usado.
- Evidencia inicial: `mapGlobalType` solo proyecta escalares, enums y sus variantes de puntero o arreglo. La selección vigente conserva candidatos de records sin colisión; queda por comprobar la dependencia introducida solo por un global. El soporte de campos de record comprueba identidad y layout de Clang.
- Presupuesto: `include/nativesharp/Interop/CppSemanticInventory.h`, `src/Interop/Cpp/CppSemanticInventory.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`, fixtures y script de `pruebas-tanda-08-record-c`, pruebas unitarias de sesión Clang si son necesarias, y registros Markdown. Conservar el modo C de la sesión para no tratar globales de clases C++ como structs C. No modificar NSLink ni LLVM. No dar por soportados globales const/volatile de record ni ABI de paso por valor sin evidencia.
- Preparar casos de objeto, puntero y arreglo donde el nombre del struct no aparezca en código NativeSharp ni en las firmas invocadas; contrastar escrituras con una función C. Implementar el conjunto antes de validar de manera agrupada; conservar rechazo de colisiones y tipos incompatibles sin romper sombras.
- La batería agrupada posterior a `a4a0b2aad` terminó con código 1: el caso `record-c` falla al mapear el carrier LLVM del record importado `GlobalOnly`; los otros seis proyectos pasaron. Investigar la declaración de almacenamiento externo en `src/LLVM/Lowering/OopLlvmStaticState.cpp`, el mapeo y el indexado, preservando las barreras de construcción y ABI por valor. La siguiente batería debe repetirse después de agrupar las correcciones.
- Presupuesto de corrección: añadir `src/LLVM/Lowering/OopLlvmStaticState.cpp`. Para almacenamiento externo de records completos usar únicamente su tamaño físico verificado y dimensiones comprobadas; no crear un carrier general de valor ni habilitar ABI o construcción por valor. Mantener trazabilidad y validación agrupada.
- Ampliación de pruebas: añadir matriz de structs y union global C al mismo fixture, comprobando las escrituras desde C. Incluir casos negativos de global record const y volatile mientras sus calificadores no se proyectan. No cambiar producción salvo que las pruebas agrupadas evidencien un defecto concreto.
- Añadir también un puntero global doble al mismo struct para comprobar indirectamente la lectura de sus dos niveles y la identidad del tipo. No habilita conversiones de punteros calificados.

### Global C const struct

- Objetivo: permitir leer campos de un struct global C const, sin permitir modificar ni obtener un puntero mutable a sus campos.
- Evidencia: `NamespaceStateSymbol.isCppReadonly` establece `readonlyReceiver`, que `MemberAccessAnalyzer.cpp` propaga y consulta para asignación, pero el campo importado aún puede ser direccionable aunque el receptor sea readonly.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, `src/Frontend/Bodies/MemberAccessAnalyzer.cpp`, fixtures y script de `pruebas-tanda-08-record-c`, y registros Markdown. Conservar rechazo de volatile struct, punteros calificados y arrays const de record hasta implementar sus contratos. Agrupar y validar los casos de lectura, escritura y dirección.

### Global C volatile struct

- Objetivo: preservar `volatile` al leer y escribir campos de un struct global C y rechazar su dirección como puntero sin calificar. No trasladar el calificador del almacenamiento de un puntero al objeto apuntado.
- Evidencia: el inventario rechaza el record volatile; el estado transporta `isVolatile`, pero el análisis de campos y `HirCompletionValueFrames.cpp` solo consideran el calificador declarado en el propio campo.
- Presupuesto: `include/nativesharp/Frontend/Internal/MethodBody/ExpressionResult.h`, `src/Interop/Cpp/CppSemanticInventory.cpp`, `src/Frontend/Bodies/IdentifierAnalyzer.cpp`, `src/Frontend/Bodies/MemberAccessAnalyzer.cpp`, `src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp`, `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionValueFrames.cpp`, fixtures de `pruebas-tanda-08-record-c` y registros Markdown. No habilitar ABI por valor ni punteros a volatile; preservar las regresiones existentes.
- Validar lectura/escritura compartida con C, IR de cargas y escrituras volatile, dirección rechazada y regresiones de arreglo y puntero con una validación agrupada.
- Resultado inicial: batería agrupada con código 0; IR confirmó cargas y escrituras volatile del struct y carga volatile del puntero con escritura ordinaria en el objeto apuntado. El diagnóstico de dirección atribuye erróneamente el caso a bit fields; precisarlo en `IdentifierAnalyzer.cpp` y exigir ambos tipos de instrucción del puntero en el script antes de repetir la validación.

### Records C globales calificados: arreglos y anidamiento

- Objetivo: importar arreglos fijos de structs C const y volatile y conservar el calificador a través de índices y campos anidados, sin propagarlo del almacenamiento de punteros a los objetos apuntados.
- Evidencia: `mapGlobalType` rechaza explícitamente los records calificados en arreglos; el análisis de índices ya propaga readonly y volatile, y HIR transporta volatile por direcciones de índices y campos. Los casos combinados no están verificados.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, fixtures y script de `pruebas-tanda-08-record-c`, y registros Markdown. Si la batería revela un defecto, ampliar el presupuesto únicamente a las rutas semánticas o HIR identificadas. Mantener rechazados punteros a volatile y ABI de records por valor no verificado.
- Preparar lectura de matriz const, escritura y lectura de matriz volatile, campos de structs anidados, comprobación C, IR y rechazos de escritura/dirección; verificar conjuntamente al finalizar el conjunto.
- Primera batería: código 1 porque `*volatile_nested_record.target = 47` produjo una carga volatile del puntero seguida incorrectamente de `store volatile` en el apuntado. En `finishStorageUnaryFrame` la desreferenciación usada como dirección devuelve el nodo de carga del puntero y su indicador se interpreta como calificador del destino. Añadir `src/Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp` al presupuesto y separar ambos nodos sin eliminar la carga volatile; repetir la validación agrupada.
- La repetición terminó con código 0 y el IR confirmó la carga volatile seguida de escritura ordinaria. El rechazo de una dirección de campo readonly sigue mencionando incorrectamente bit fields. Añadir `src/Frontend/Bodies/IdentifierAnalyzer.cpp` al presupuesto y dar un diagnóstico de readonly; comprobar el texto en ambos fixtures const y exigir en el IR la carga volatile del campo puntero anidado.

### Global C puntero a struct const

- Objetivo: proyectar un global C `const struct*`, conservando la inmutabilidad del apuntado en accesos y conversiones; no confundirla con const del propio puntero.
- Evidencia: `mapGlobalType` rechaza explícitamente el record const aun cuando `NamespaceStateSymbol.cppPointerPointeeReadonly` y el análisis de desreferenciación ya transportan readonly. La ruta no está verificada de extremo a extremo.
- Presupuesto: `src/Interop/Cpp/CppSemanticInventory.cpp`, fixtures y script de `pruebas-tanda-08-record-c`, y registros Markdown. Si las pruebas revelan errores de semántica o HIR, registrar antes los archivos concretos. No habilitar punteros a volatile ni niveles de indirección calificados no representados.
- Preparar lectura y comprobación C, rechazo de escritura al campo y de conversión a puntero mutable, y un caso positivo del puntero mutable existente; validar el conjunto y las regresiones agrupadamente.
- Ampliar el mismo fixture con el paso del puntero readonly a una función C `const struct*`, la asignación de un puntero mutable al global readonly y el rechazo de pasarlo a una función que requiere `struct*`. Comprobar la causa exacta del rechazo.

### Resultado C puntero a const

- Objetivo: permitir resultados de funciones C `const T*` cuando T sea escalar o struct proyectado, conservando readonly al desreferenciar, pasar o guardar el resultado.
- Evidencia: `projectFunction` prohíbe el apuntado const en resultados; `NamespaceFunctionSymbol.cppReturnType` lo conserva, pero el adaptador de llamadas no lo transfiere a `declaredPointerPointeeReadonly`.
- Presupuesto: `src/Interop/Cpp/CppSemanticFunction.cpp`, `src/Frontend/Bodies/MethodBodyBuildCoordinator.cpp`, fixtures y script de `pruebas-tanda-08-record-c`, y registros Markdown. No habilitar punteros a volatile ni niveles anidados no representados; preservar llamadas NativeSharp y barreras de ABI.
- Verificar lectura de resultado escalar y struct, paso a función const y rechazos de escritura, conversión mutable y paso a función mutable. Ejecutar el conjunto agrupado y registrar causas exactas.
- La primera batería terminó con código 1: los proyectos mixtos pasaron, pero la suite aún exigía rechazar `tools::pointer`, que retorna `const int*`. Añadir `tests/unit/CppClangSessionTests.cpp` al presupuesto para comprobar que el resultado conserva el tipo de Clang y mantener el rechazo de los casos interno e incompleto.

### Requisito de globales C locales por hilo

- Objetivo: comprobar la infraestructura necesaria para T08-004/T08-015 antes de proyectar globales C TLS. `CppGlobalDeclaration.threadLocal` se captura, pero el inventario lo rechaza; no se ha demostrado que el enlazado y el arranque preserven almacenamiento por hilo.
- Presupuesto: nuevo fixture `nativesharp-interop/pruebas-tanda-08-tls-c` y registro Markdown. Salidas solo en `A:/nsc-interop-build/tanda08-tls-c`. Usar Clang oficial y NSC/NSLink existentes; NSLink y el árbol LLVM son de solo lectura.
- Una biblioteca C con `__declspec(thread)` ofrece una función que verifica valor inicial e independencia entre hilo principal y un hilo Win32. NativeSharp llama esa función sin importar todavía el global. Registrar por separado compilación, enlace y ejecución; un fallo no autoriza modificar NSLink. No incluir el fixture como gate de la batería hasta conocer el comportamiento.
- Primera ejecución: Clang compiló, pero el enlace se detuvo en `__imp_GetExitCodeThread`, importación del instrumento que no está en el catálogo actual. Sustituirla por un resultado escrito por el hilo y leído tras `WaitForSingleObject`, sin alterar el objetivo de aislamiento; volver a medir.
- Segunda ejecución: Clang compiló y NSLink falló con `NSL1020` para `_tls_index`. El objeto contiene `.tls$` y relocaciones AMD64 SECREL; la fuente consultada del enlazador reconoce SECREL, pero no se ha demostrado el directorio TLS ni su inicialización. No modificar NSLink; registrar el requisito y continuar los demás contratos mientras se investiga una solución dentro de NSC.

### Optimización de proyecto mixto de records C

- Objetivo: comprobar que las operaciones y calificadores ya probados en `pruebas-tanda-08-record-c` siguen funcionando al compilar NativeSharp con O2.
- Presupuesto: `nativesharp-interop/pruebas-tanda-08-record-c/validar.cmd` y registro Markdown. Conservar inspección IR O0 y pruebas negativas; generar y ejecutar un segundo binario O2 contra la misma biblioteca C. No inferir cobertura de otras arquitecturas ni de todas las optimizaciones.
- Ampliación: ejecutar también O1 y O3 en el mismo conjunto, usando el mismo objeto C, para comprobar las cuatro opciones de NSC sin reconstruir el compilador por cada caso.

### Global enum const

- Objetivo: comprobar lectura de un global C `const enum` y rechazo de su escritura desde NativeSharp.
- Presupuesto: `Api.h`, `Api.c`, `Main.ns`, `ConstWrite.ns.txt` y `validar.cmd` de `nativesharp-interop/pruebas-tanda-08-enum-funcion`, más registro Markdown. No modificar producción sin observar el resultado.
- La lectura y el rechazo deben ejecutarse con la batería agrupada. Un rechazo de compilación no basta para cerrar el soporte completo de calificadores.

### Global puntero a enum C

- Objetivo: comprobar lectura y desreferencia de un global C de tipo puntero a enum con el enum ya seleccionado por una función.
- Presupuesto: `Api.h`, `Api.c` y `Main.ns` de `nativesharp-interop/pruebas-tanda-08-enum-funcion`, más registro Markdown. La selección de un enum referido únicamente a través de un puntero global es un caso diferente y sigue pendiente.
- Validar junto con la tanda, sin tratar un único caso como cierre general de punteros o globales.
# Ampliación dirigida de union C

Ampliar el fixture `nativesharp-interop/pruebas-tanda-08-union` y su registro
para probar lecturas y escrituras superpuestas en ambos sentidos y ejecutar el
caso en O0–O3. Mantener los resultados en `A:/nsc-interop-build/tanda08-union`.
No modificar producción sin identificar y registrar antes el alcance necesario.

# Campos de bits C en Windows x64

Crear `nativesharp-interop/pruebas-tanda-08-bitfields-c` y su registro para
contrastar con Clang lectura y escritura de campos de bits C con y sin signo,
campos de anchura cero, almacenamiento compartido y `pack(1)`; salidas solo en
`A:/nsc-interop-build/tanda08-bitfields-c`. No cambiar producción hasta
localizar el contrato y registrar los archivos afectados.
Para campos C no volatile de struct en Windows x64, transportar coordenadas
de bits derivadas del AST y un acceso por ventana de bytes comprobada. Permitir
`include/nativesharp/Interop/CppSemanticRecord.h`,
`include/nativesharp/Frontend/CppImportedInstanceField.h`,
`include/nativesharp/Frontend/ClassMemberSymbol.h`,
`src/Interop/Cpp/CppSemanticRecord.cpp`, `src/Frontend/ProjectMemberModel.cpp`,
`src/Frontend/ProjectObjectLayoutModel.cpp` y
`src/LLVM/Lowering/OopLlvmFieldsAndConversions.cpp`; conservar la protección
de direcciones y rechazar casos cuyo acceso seguro no esté demostrado. Los
campos volatile, unions y formas no representadas permanecen pendientes.

# Contrato de long double C en Windows x64

Crear `nativesharp-interop/pruebas-tanda-08-flotante-c` y registrar evidencia
para comprobar el formato efectivo de `long double` en Clang 23 y su uso por
argumento, resultado y campo desde NativeSharp, con O0–O3. Salidas en
`A:/nsc-interop-build/tanda08-flotante-c`. No cambiar producción hasta
comprobar representación y delimitar el alcance.
El caso confirmado en Windows x64 usa IEEE binary64 y ABI double, mientras
NSC rechaza el typedef. Permitir `CppType.h`, `ClangSession.cpp`,
`CppScalarTypeMapping.cpp`, `CppAliasBuiltinType.h` y los registros para
capturar explícitamente semántica IEEE binary64 desde Clang y usarla al
mapear `long double`. No inferir el formato solo por el ancho. La persistencia
de este dato en metadata queda pendiente de diseño y validación propia; no
declarar soporte de bibliotecas precompiladas hasta resolverla.
Para el fallo confirmado, permitir `src/Interop/Internal/CppCRecordMetadataCodec.cpp`,
`src/Interop/Internal/CppCRecordMetadataCodec.h`, `src/Interop/LibraryMetadataPayload.cpp`,
`src/Interop/LibraryMetadataVerification.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`,
`src/Interop/Cpp/CppSemanticInventory.cpp`, `include/nativesharp/Interop/LibraryMetadata.h`
y los controles existentes para una extensión versionada y canónica del
catálogo. Preservar lectura y huellas anteriores, incluir formato flotante en
comparación y fingerprint sin procedencia y rechazar metadata incompatible.
Permitir además `src/Interop/LibraryMetadataConstruction.cpp`: el productor
elige por defecto la variante anterior y debe promoverla solo si el catálogo
real contiene formato IEEE; preservar la versión anterior cuando no lo usa.
Ampliar el control escalar existente en `tests/unit/CppClangSessionTests.cpp`
para distinguir tamaño 64 sin evidencia IEEE y formato IEEE acreditado.
Preparar en el fixture flotante un productor NativeSharp precompilado y un
consumidor con el mismo encabezado para detectar pérdida del formato capturado.

# Punteros a función C devueltos al consumidor

Ampliar `nativesharp-interop/pruebas-tanda-08-callback` y su documentación
para probar el retorno de un puntero a función C, su llamada desde NativeSharp
y su tránsito de vuelta a C. Ejecutar la matriz O0–O3 agrupada. No modificar
producción sin delimitar primero el archivo y el defecto observados.
Ampliar además `pruebas-tanda-08-meta-c` con productor NativeSharp
precompilado y consumidor que compartan un callback C y un struct que lo
almacena. Probar retorno, campo y llamada en O0–O3; limitar edición al
fixture, scripts y registros hasta observar cualquier defecto concreto.
La prueba produjo metadata pero el consumidor omitió el método; la
materialización de tipos no reconoce `fn:`. Permitir la reconstrucción de
prototipos de puntero C en `src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp`,
conservando el rechazo de efectos y formas que no se puedan reconstruir y
registrando las limitaciones y pruebas. No modificar el formato NSMETA.
Ampliar esa reconstrucción a convenciones C admitidas por el parser y a
variadicidad válida, manteniendo efectos y carriers no representados como
rechazo. Extender `pruebas-tanda-08-meta-c` con callbacks vectorcall y C
variádico y comprobar el consumidor real en O0–O3.
Agregar un proyecto separado para leer e invocar un puntero a función desde un
campo de struct C y desde un global C, sin alterar el caso ya validado.
Extender ese proyecto para escribir callbacks en el campo y en el global y
comprobar su invocación desde C; agregar controles de lectura y rechazo de
escritura para un global y un campo con puntero a función const.
Preparar en el mismo fixture un proyecto separado para callbacks C
almacenados en un global y campo volatile, con intercambio de escrituras entre
C y NativeSharp. Comparar también su IR frente a Clang en Windows x64 antes de
atribuir soporte a las reglas de orden volatile; no modificar producción sin
identificar y registrar primero las rutas concretas.
Clang confirmó acquire/release para lectura/escritura de punteros a función
volatile alineados. Permitir cambios en `src/Interop/Cpp/CppSemanticFunction.cpp`,
`src/Frontend/HIR/ExpressionStatementMachine/HirNameExpressionLowering.cpp`,
`src/LLVM/Lowering/PackedArrayAlignment.h`, `OopLlvmExpressionMachine.cpp` y
`OopLlvmArraysAndStores.cpp` para proyectar y transportar únicamente ese
contrato cuando el modo MS y alineación estén probados. Extender el fixture y
sus comprobaciones de IR; conservar accesos subalineados sin orden atómico.
La primera comparación encontró que el selector LLVM de globales excluye la
identidad estructural `fn:`. Permitir el ajuste de `OopLlvmSupport.h` para
incluir ese tipo bajo las mismas condiciones de modo, anchura y alineación.
Para el fallo observado, reutilizar el mapeo de firma de funciones existente
en `include/nativesharp/Interop/CppSemanticFunction.h` y
`src/Interop/Cpp/CppSemanticFunction.cpp`, y conectarlo a campos C en
`CppSemanticRecord.cpp` y globales C en `CppSemanticInventory.cpp`. Conservar
rechazo de volatile y firmas no representables; añadir registros Markdown.
La primera construcción mostró que el layout de campo no reconoce el tipo
estructural de función. Permitir `src/Frontend/ProjectObjectLayoutModel.cpp`
para representar ese tipo como puntero nativo, más su registro y validación.

Extender el mismo fixture con parámetros y retornos C de unions de 4 y 12
bytes, para contrastar la clasificación ABI directa e indirecta en Windows
x64. Registrar el resultado sin atribuir compatibilidad general a la muestra.
Agregar un caso de 8 bytes con miembro flotante y otro con un struct anidado,
incluyendo acceso y paso por valor; conservar ejecución agrupada O0–O3.

Ampliar `pruebas-tanda-08-meta-c` con un productor precompilado y un consumidor
que comparten un encabezado con union C, por puntero y por valor. Permitir el
script, los archivos de fixture y los registros; no modificar producción hasta
identificar un defecto y registrar por separado los archivos necesarios.

