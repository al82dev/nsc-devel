# Tanda 08: declaraciones, nombres y tipos C

Estado: auditoría inicial; ningún ítem se cierra solo por observar datos en Clang.

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
