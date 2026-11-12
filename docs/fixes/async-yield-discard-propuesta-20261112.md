# Propuesta: propiedad del resultado de yield al cancelar

## Controles tras enlazar los hechos por rama

Después del build incremental de `nsc` con los hechos por rama, los runners
`cancel-drop-after-branchfacts-20261112.log`,
`future-after-branchfacts-20261112.log` y `views-after-branchfacts-20261112.log`
terminaron con salida 0 en `A:/nsc-fixes-evidencia`. Estos controles no
demuestran que los hechos sean suficientes para destruir un valor ni que la
cancelación anidada esté corregida.

## Repetición del modo de item sin guard experimental

Tras retirar el guard experimental de alias que rechazaba un positivo, se
reconstruyeron incrementalmente `nsc` y el target dirigido de caché. El runner
`tests/integration/async_yield_cross_process_cache.py` se repitió en
`A:/nsc-fixes-evidencia/async-yield-itemmode-isolated-20261112`: cold, warm,
Unknown y cuatro workers devolvieron 0; los 34 registros cold, warm y paralelo
coincidieron y se observó Unknown. `comparison.log` y los logs por fase
conservan el resultado. Esto separa esta validación del guard retirado; aún no
demuestra la clasificación de ownership ni la corrección de cancelación.

## Reproducción tras reanudar el trabajo

El 12 de noviembre se ejecutó desde `A:/nsc-devel-fixes` el runner
`A:/nsc-fixes-evidencia/nested-cancel-regression-20261112.cmd`, que configura
los temporales y las bibliotecas de runtime de `A:/nsc-fixes-evidencia/build`
y utiliza `build/bin/nsc.exe` de esa copia. `nested_cancel_first` terminó con
salida 22 en O0 y O2: después de cancelar, el siguiente avance todavía devuelve
un elemento. El control `nested_normal` terminó con salida 42 en O0 y O2.
El árbol contiene los cambios async sin registrar descritos en esta campaña;
estos resultados no acreditan el commit base aislado ni una corrección.

Una primera invocación sin las variables del runtime aislado falló antes de
compilar al buscar una biblioteca en `C:/Program Files (x86)/NativeSharp`;
no se considera resultado del caso ni se modificó esa instalación.

## Problema demostrado

La copia de cleanup que se ejecuta después de `Cancel()` puede llegar a un
segundo `yield`. Publicarlo produce `true`, aunque el contrato aprobado requiere
que el avance termine con `false`. El resultado no se puede ignorar: su
expresión puede tener efectos, lanzar, transferir un dueño o devolver un
préstamo. En un condicional válido una rama devuelve un `owned` y la otra un
objeto compartido; el MIR actual une solo el valor. Fuentes, comandos y límites
del ensayo están en `async-yield-mixed-prototype-20261112.md` y en
`A:/nsc-fixes-evidencia/async-yield-discard-design-20261112.md`.

## Representación propuesta, todavía no implementada

Anotar **cada productor relevante del resultado** del operando de un yield,
no solo el nodo yield, con una clasificación derivada del binding del callable
cerrado:

| Clasificación | Condición que debe demostrarse | Tratamiento al descartar |
|---|---|---|
| `NoDestruction` | Escalar, préstamo o referencia, o alias de dueño sin transferencia | Evaluar una vez; no destruir el referente. |
| `OwnedValue` | Valor construido o transferido con drop glue y vida propia | Materializar una vez y destruir mediante dirección y marca de vida. |
| `OwnedObjectAndStorage` | Transferencia/construcción propia de objeto y storage | Destruir y liberar según el contrato del productor. |
| `OwnedObjectBorrowedStorage` | El objeto es propio y el storage es prestado | Destruir en sitio sin liberar storage, si esa forma está demostrada. |
| `BranchDependent` | Una expresión selecciona productores con clasificaciones distintas | Conservar la rama realmente ejecutada y aplicar su tratamiento. |
| `Unknown` | Faltan binding, identidad, contrato o evidencia suficientes | Mantener visible; no interpretarlo como préstamo, dueño ni éxito. |

`Owned` por sí mismo no equivale a transferencia: el binding de `yield return
owner` sobre un local owned es `Owned`, pero `transfer=false`; destruir ese
alias y luego el local destruiría dos veces. `new Payload()` tiene
`fresh=true` sin `ownership` ni `transfer`; `new Box()` tiene categoría
temporal sin esos tres campos. El contexto de item `ref`/`ref mut` debe
predominar sobre el tipo y el ownership del referente.

La información semántica debe obtenerse mediante
`ProjectMethodBodyModel::expressionTypeBinding(syntax, callableOrdinal)`, no el
índice HIR solo por sintaxis. `completeScheduledCoroutineValueStatementFrame`
ya conoce `frame.memberOrdinal` y el árbol HIR final del operando; es una
frontera candidata para adjuntar la información sin modificar el índice común.
La consulta por ordinal funcionó en el sondeo de `Generator<int>` y
`Generator<Payload>`; no demuestra todos los genéricos, lambdas o funciones de
namespace. El modo `ref` declarado se determina al crear el frame del yield.

## Flujo por ramas

Para `Conditional`, `MatchExpression`, coalescencia y otras expresiones que
seleccionen un resultado, cada rama conserva su propia procedencia. La emisión
en la **copia cancelada** puede consumir el resultado dentro de la rama, o bien
unir valor y marca de propiedad con valores `phi` correspondientes a los mismos
predecesores. En ambos casos debe evaluar la condición y la rama elegida una
sola vez, respetar las ramas que no alcanzan la unión y propagar excepciones.
No se vuelve a evaluar una condición para recuperar ownership.

La clasificación debe seguir conversiones, inicializadores y productores
concretos sin atribuir a ciegas el binding de un wrapper a sus hijos. En
particular, `FullExpressionCleanup` y `OwnedTemporaryBorrow` ya establecen
dueños y orden de limpieza: un descenso especial no puede omitirlos ni liberar
dos veces. Las expresiones condicionales de structs con drop y los resultados
de funciones, arreglos, delegados, strings y placement requieren pruebas de
su contrato antes de declararlos clasificados. El tratamiento de `Unknown`
no está decidido: rechazar el compilado durante la emisión de la ruta cancelada
también rechaza programas que solo avanzan normalmente, y tratarlo como
préstamo o volver a publicar un elemento es incorrecto. La propuesta no autoriza
introducir ninguno de esos fallbacks silenciosamente.

## Excepciones y suspensión

Un productor que lanza antes de crear el valor no debe provocar su drop; un
valor ya materializado debe tener un dueño y una marca de vida antes de una
operación posterior que pueda lanzar. Si el cleanup reemplaza una excepción
pendiente, debe liberar la excepción desplazada una vez, propagar la nueva y
conservar la ruta de excepciones extranjeras. El fixture lanzador actualmente
termina con `0xe04e5301`; el prototipo acotado no lo corrige.

Una suspensión necesita una continuación que sobreviva en el frame: operación
pendiente, valor/marca si ya existen y regiones de cleanup restantes. Una
marca SSA local o flag usado solo al emitir C++ no demuestra esa persistencia.
El reingreso no puede duplicar condiciones, construcción ni destrucción. El
verificador rechaza actualmente un `await` en finally anidados por dominancia
de cleanuppad, y `Drop()` hace un único poll antes de liberar el frame; ambas
fronteras permanecen abiertas y se prueban separadamente.

## Primer cambio verificable propuesto

1. Transportar al HIR y MIR, exclusivamente en el árbol de operandos de
   `AsyncYieldValue`, los hechos del binding correcto y el modo de item; por
   defecto usar `Unknown`, sin activar todavía descarte ni alterar diagnósticos
   o ejecución. Mantener la distinción por productor y marcar explícitamente
   las uniones dependientes de rama.
2. Ampliar la prueba dirigida existente para verificar la información en HIR y
   MIR: move, `new`, fábrica, alias de local owned, campo compartido, ref,
   struct con drop, condicional mixto y las dos especializaciones ya medidas.
   Incluir binding ausente y wrappers, sin suponer clasificación cuando falta.
3. Comprobar que los controles de avance normal y los diagnósticos anteriores
   permanecen iguales. Ese cambio sería solo infraestructura semántica; no
   resuelve ni cierra la cancelación.
4. En una tanda posterior, emitir descarte en la copia cancelada para las
   categorías y ramas demostradas, con fixtures que identifiquen por ID cada
   destrucción, ambas ramas y efectos. Resolver explícitamente `Unknown` antes
   de dar la ruta por completa; integrar await/SEH cuando sus precondiciones
   estén demostradas.

Esta propuesta no modifica genéricos ajenos ni el comportamiento de avance
normal. No sustituye las pruebas de valores movidos, préstamos, excepciones,
await y abandono que faltan para cerrar la limitación.

## Alternativas de almacenamiento pendientes de evaluar

Un campo opcional en cada nodo HIR y MIR simplifica transportar hechos por
productor, pero aumenta la memoria de todos los nodos aunque no pertenezcan a
yield. Una tabla lateral por modelo solo asignaría entradas a los operandos
pertinentes; requiere reconstruir o serializar entradas al restaurar MIR desde
`MirCache.cpp`, que crea nodos nuevos. Ninguna de las dos alternativas está
aprobada como implementación.

Otra posibilidad es consultar a demanda el binding semántico desde el modelo
MIR: su build conserva referencia al modelo HIR, y el codec de caché conserva
la sintaxis mediante índices reubicables. Esto evitaría campos adicionales en
cada nodo, pero debe demostrarse para nodos sintéticos, wrappers, restauración
de caché, lifetime de modelos, concurrencia e identidad de caché/codegen. Los
fixtures de clase no cubren funciones de namespace. En la ruta fuente actual,
`initializeNamespaceFunction` asigna a `memberOrdinal` el ordinal semántico
`members().size() + functionOrdinal`; `createAsyncGeneratorPoll` lo copia al
poll, y MIR copia ambos campos. Por tanto `currentMirFunction.memberOrdinal`
corresponde al ordinal de esa ruta, en lugar de ser siempre incorrecto como
afirmaba una revisión anterior de este documento. Falta demostrar esa relación
con pruebas de namespace, restauración entre procesos, variaciones del grafo,
lambdas y modelos precompilados; `namespaceFunctionOrdinal` debe mantenerse
como identidad diferenciada. Si no hay binding o identidad estable, el
resultado permanece `Unknown`. No se ha cambiado el acceso a modelos ni se
ha elegido una de estas alternativas.

Una variante de la tabla lateral construiría los hechos en una pasada serial
después de que `lowerFunctions` instale las funciones restauradas, espere el fin
de los workers, mueva todas a `functions_` y ajuste los frames. El mapa sería
inmutable durante codegen y sus entradas solo cubrirían productores de yield.
Las claves de puntero solo vivirían en esa instancia del modelo: al restaurar
una función se reconstruirían a partir de la sintaxis reubicada y el binding del
callable. `ProjectOopMirCacheIdentity::writeNodeFlags` hoy no incluye los campos
del binding consultados externamente; la identidad de codegen debería incorporar
los hechos relevantes en un orden estable, no direcciones. Faltan pruebas de
lifetime, modelos sin frontend, wrappers, `Unknown`, caché y generación
paralela. La existencia de la pasada no es todavía una corrección.

### Análisis posterior: tabla por función y caché

`MirNodeMachine::lower` conserva actualmente la forma del árbol HIR en MIR:
visita cada hijo en orden, copia los nulos y crea un nodo MIR por nodo HIR.
`copyIdentity` copia la referencia de sintaxis. Esto permite investigar una
tabla lateral por función de entradas `(yield, camino de hijos, hecho)`, donde
el camino distingue wrappers y productores que comparten sintaxis. Debe
comprobarse la correspondencia al reconstruirla; no es un contrato demostrado
para transformaciones futuras. Una clave solo por sintaxis o rango no basta.

`MirCache.cpp` ya vuelve a enlazar sintaxis desde la tabla HIR actual y usa
índices preorden para referencias de acciones de cleanup; no conserva punteros
de otra ejecución. `ProjectCompilation::compileProjectOopMir` no activa esa
caché en el recorrido normal; la prueba actual llama explícitamente
`setMirCacheRoot`. Por ello, ejecutar el CLI dos veces no demuestra una
restauración MIR entre procesos. Si la tabla propuesta se calcula otra vez con el binding
actual después de restaurar las funciones MIR, sin modificar el árbol ni
serializar sus hechos en ese payload, la clave prelower no requiere esos hechos
por sí solos: el mismo árbol restaurado se puede volver a clasificar. Esta es
una inferencia condicionada a demostrar que ninguna etapa de construcción MIR
depende de ellos y que cada camino se reconstruye inequívocamente. Si los hechos
se serializan o influyen en la construcción MIR, la clave y el formato de esa
caché deben incluirlos y versionarse.

La caché de objetos pre-LLVM sí requiere incorporar los hechos a la identidad
de definición: `composeModuleCacheKeys` crea `ProjectOopMirCacheIdentityIndex`
antes de emitir el módulo, y `computeFunctionDigests` utiliza
`writeFunction(..., true)` para cada definición. El vector debe serializarse
determinísticamente por función y camino, sin direcciones ni ordinales efímeros
usados como identidad entre procesos. La caché posterior a LLVM deriva su
clave del texto IR generado. La ruta aún no se ha implementado ni probado.

La comprobación inicial puede limitarse a una tabla de prueba, sin modificar
los nodos de producto ni la generación: comparar las rutas y bindings de
operandos `move`, nueva construcción, fábrica, alias, préstamo y condicional
mixto; incluir `FullExpressionCleanup`, `OwnedTemporaryBorrow`, síntesis sin
sintaxis, namespace y genéricos. Comparar cold/warm, uno y varios workers y
dos procesos reales, con recargas y cambio deliberado de binding/identidad;
verificar que los datos no dependen de direcciones, que `Unknown` permanece
explícito y que el programa y sus diagnósticos no cambian. La prueba actual
solo demuestra restauración dentro del mismo proceso, no estos escenarios.
Para la prueba entre procesos se necesita un harness dirigido que reciba una
carpeta de caché compartida y ejecute dos procesos distintos: el segundo debe
acreditar mediante el contador de restauraciones que cargó el poll, reconstruir
las entradas desde su propia sintaxis y binding y comparar una representación
estable. El harness debe preservar la misma revisión y configuración y no
depender de la caché del bundle u objetos como sustituto de la caché MIR.

Se preparó `tests/unit/AsyncYieldCrossProcessCacheTests.cpp` como harness de
este ensayo. Su primera versión reutilizó el fixture mixto
de una clase, recibe un directorio exclusivo y una fase `cold` o `warm`, y
exige respectivamente cero o al menos un poll restaurado por la caché MIR
activada en el propio modelo. Emite símbolo, ruta de hijos y hechos de binding
de ambas ramas para comparar dos procesos sin comparar direcciones. El primer
proceso debe usar un directorio nuevo; el segundo debe conservar ese directorio
y ejecutarse por separado. La primera versión se compiló en un target dirigido
y se ejecutó en dos procesos distintos: el frío registró cero polls
restaurados y el cálido registró uno; ambos terminaron con salida 0. Las dos
líneas `producer` coincidieron exactamente. Evidencia:
`A:\nsc-fixes-evidencia\async-yield-cross-cold-20261112.log` y
`A:\nsc-fixes-evidencia\async-yield-cross-warm-20261112.log`.

Posteriormente se amplió el archivo de prueba con una función de namespace de
ramas mixtas y un yield de namespace con `FullExpressionCleanup` y
`OwnedTemporaryBorrow`. La ampliación imprime las rutas de wrappers dentro
de operandos de yield y mantiene explícito `UNKNOWN` si falta binding. El
target se compiló incrementalmente y la ampliación terminó con salida 0 en
dos procesos; el frío restauró cero polls y el cálido tres. Cuatro líneas de
productores y dos de wrappers coincidieron exactamente entre ambos procesos.
Los logs son `A:\nsc-fixes-evidencia\async-yield-cross-expanded-cold-20261112.log`
y `A:\nsc-fixes-evidencia\async-yield-cross-expanded-warm-20261112.log`.

Se preparó el runner `tests/integration/async_yield_cross_process_cache.py`
para ejecutar ambos procesos sobre un directorio nuevo en A y comparar
automáticamente las líneas `producer` y `wrapper`, excluyendo el conteo que
necesariamente difiere entre fases. Guarda comandos, salidas y comparación en
ese directorio. Una ejecución dirigida terminó con `cold_exit=0`,
`warm_exit=0`, seis registros por fase e igualdad verdadera. El frío restauró
cero polls y el cálido tres. Comandos y resultados están en `cold.log`,
`warm.log` y `comparison.log` bajo
`A:\nsc-fixes-evidencia\async-yield-cross-runner-20261112`; una segunda
ejecución independiente produjo lo mismo en el directorio con sufijo
`-agent-20261112`. El ejecutable de prueba usado en ambas fue
`A:\nsc-fixes-evidencia\build\tests\nsc_async_yield_cross_process_cache_tests.exe`,
SHA-256 `b5c63fbda38c634c3a5b2d180fa7747c5a3668d6b50b72513668d5802e4cfa1a`.
En ese momento no se habían verificado workers paralelos, cambios de grafo ni
invalidación de identidad de objeto. Los hechos registrados son evidencia de binding, no una clasificación
de propiedad ni una solución del descarte.

### Infraestructura inicial de hechos

Se añadió un vector lateral por función MIR, con rutas del yield y del nodo
observado dentro del operando. Se reconstruye después de instalar funciones
nuevas o restauradas y consulta el binding de la sintaxis con el ordinal del
callable. Guarda copias de hechos, no punteros a nodos ni a bindings; el campo
`hasBinding=false` representa ausencia de binding. Son observaciones crudas:
una entrada también puede representar un argumento o un wrapper, por lo que no
significa automáticamente que sea propietaria del resultado del yield.

La primera compilación dirigida falló porque se intentó leer `isAsyncPoll`
desde la función MIR, que no tiene ese campo. Se corrigió usando la función
HIR correspondiente; la compilación incremental posterior pasó. El runner
de procesos independientes terminó con salida 0 en ambas fases y comparó 26
registros iguales: 20 hechos, cuatro productores y dos wrappers. La fase fría
restauró cero polls y la cálida tres. Evidencia y comandos:
`A:\nsc-fixes-evidencia\async-yield-facts-20261112\cold.log`, `warm.log` y
`comparison.log` en la misma carpeta.

Ese fixture no contiene una entrada sin binding: no demuestra por sí solo
el camino `Unknown`. Tampoco probó todas las especializaciones genéricas,
`ref`, cambios de grafo, concurrencia, ni la semántica de las conversiones y
wrappers. La tabla no se usa en el codegen y no modifica el comportamiento de
cancelación ni la identidad de objetos. Si posteriormente se usa para emitir
código, la identidad pre-LLVM y sus pruebas deberán actualizarse antes de
activar ese consumo; este experimento no valida esa futura identidad.

La primera representación guardaba una ruta completa por hecho, lo que
costaba la suma de profundidades y podía ser cuadrático en un árbol profundo.
El código se cambió después a una sola pasada preorden con índices por
función, referencia al hecho padre e índice de hijo. Las raíces comparten el
índice del yield; no se guardan punteros. Para N nodos visitados, F hechos y B
bytes copiados de bindings, el coste esperado es O(N+B) tiempo y O(N+F+B)
espacio máximo incluyendo la pila de trabajo. Los índices identifican el árbol
de esa función, no constituyen identidades globales. La pasada observa todos
los descendientes del operando, incluidos argumentos y wrappers; un binding
observado no clasifica por sí mismo el resultado como propio o prestado.

Se amplió el harness para una fase `unknown` separada que quita la sintaxis
de un nodo solo en el HIR construido para la prueba, exige un hecho sin binding
y conserva `Unknown`. Añade fixtures para especializaciones genéricas y un
item `ref`, además de verificar los enlaces entre padre e hijo. El target
dirigido compiló incrementalmente y el runner terminó con salida 0 en frío,
cálido y `unknown`. Las dos primeras fases produjeron 31 registros idénticos;
la tercera observó una entrada `UNKNOWN`. Cada fase registró dos hechos de la
especialización `int`, dos de `Payload` y uno del fixture `ref`. Comandos y
resultados están bajo `A:\nsc-fixes-evidencia\async-yield-linear-20261112`.
Esta evidencia identifica el operando del fixture `ref`; no demuestra todavía
que la tabla transporte o interprete su modo de item. Los 26 registros de la
sección anterior corresponden a la representación previa de rutas completas.

Se ejecutó una ampliación dirigida que compara los registros
canónicos de una compilación con un worker frente a otra independiente con
cuatro workers, y conserva las fases fría, cálida y `unknown`. Como
`ProjectOopMirModel::initializeBuild` declara las instancias de un solo uso,
el mismo modelo no admite una reconstrucción exitosa: el harness intentó un
segundo `build`, obtuvo el diagnóstico de single-use y comprobó que todos los
hechos seguían iguales. Las cuatro fases finalizaron con salida 0; se
compararon 31 registros idénticos entre uno y cuatro workers, con `Unknown`
observado en su fase separada. Los cuatro logs confirman el rechazo y la
conservación de hechos. Evidencia:
`A:\nsc-fixes-evidencia\async-yield-parallel-20261112\comparison.log` y los
logs `cold`, `warm`, `unknown` y `parallel` del mismo directorio.
La reconstrucción aceptada de caché sigue siendo
la de un modelo nuevo en otro proceso, no una reutilización del mismo objeto.

### Modo declarado del elemento

El poll HIR conserva ahora el modo `Value`, `Reference` o `MutableReference`
derivado del tipo de retorno cerrado del generador antes de separar su cuerpo.
Si ese tipo no identifica un elemento de iterador async, se conserva `Unknown`.
Cada hecho MIR copia el modo del HIR correspondiente cuando se construye o se
restaura el cuerpo; no deduce la propiedad del elemento desde el binding de un
operando. El enum `ProjectOopYieldItemMode` y el campo de la tabla son datos
internos: LLVM todavía no los consume y la cancelación no cambia por ellos.

La compilación incremental coordinada del target de prueba terminó con salida
0 e incluyó trabajo concurrente sin registrar del guard de alias de vistas;
por eso esta evidencia no certifica el cambio de modo aislado. El ejecutable
`A:/nsc-fixes-evidencia/build/tests/nsc_async_yield_cross_process_cache_tests.exe`
tuvo SHA-256 `7eda9c67d715b4829debbc932bc8fcba79de969581c92a81687ad7d9bfa8ff72`.
El runner `tests/integration/async_yield_cross_process_cache.py` comparó el
HIR y los hechos MIR en frío, restauración de caché en otro proceso, un caso
sintético sin binding ni modo y una compilación independiente con cuatro
workers. Las cuatro fases devolvieron 0; frío y caliente produjeron 34
registros idénticos, y la fase paralela produjo los mismos 34. El proceso
cálido restauró nueve polls; el frío, cero. Los logs cuentan explícitamente un
hecho por cada combinación de clase/namespace y `ref`/`ref mut`, además de las
especializaciones genéricas `int` y `Payload` por valor. El modo `Unknown`
sintético se observó en ocho hechos y la ausencia de binding en uno. Comandos,
salidas y comparación están en `A:/nsc-fixes-evidencia/async-yield-itemmode-20261112`.

Esta comprobación cubre las firmas del fixture y la reconstrucción MIR, no
demuestra todavía todas las formas de genericidad, lambdas, bindings importados
o cambios de grafo. El valor copiado tampoco determina por sí mismo quién
destruye cada productor ni acredita la identidad de objetos pre-LLVM necesaria
si el modo se usa posteriormente en codegen.

### Clasificación inicial de resultados, sin emitir descarte

La clasificación en la tabla MIR se compiló incrementalmente y se verificó con
las pruebas dirigidas indicadas abajo. Solo examina la raíz del operando cuando
dispone de binding y no hay
transferencia, dueño fresco ni ownership de objeto o almacenamiento. Reconoce
como `TrivialScalar` constantes o lecturas directas de local/parámetro con
tipo numérico incorporado o bool y tipo MIR idéntico al binding. Reconoce como
`BorrowedReference` una raíz `LocalAddress` cuyo item se declara `ref` o
`ref mut`. Las direcciones de parámetros permanecen `Unknown` hasta disponer de
un control específico.
Los otros nodos, incluidos descendientes, wrappers, condicionales, productores
con ownership o binding ausente, y el modo desconocido, permanecen `Unknown`.

`BorrowedReference` indica únicamente que el resultado es una referencia al
almacenamiento, no que su lifetime sea válido ni que pueda descartarse. Ninguna
de las tres clasificaciones autoriza omitir la evaluación, efectos, excepciones
o limpieza de expresión. La clasificación no se consume en LLVM y no cambia el
comportamiento de `Cancel()`; antes de consumirla habrá que actualizar la
identidad pre-LLVM y verificar los hechos y contratos necesarios para cada
categoría adicional.

La prueba dirigida ampliada registró salida 0 en las ejecuciones fría, cálida,
con modo desconocido y paralela; las tres ejecuciones comparables produjeron
41 registros idénticos. La fría restauró 0 polls y la cálida 14; el control de
modo desconocido observó `Unknown`. Los contadores incluyeron 10 escalares,
entre ellos un literal bool, un literal double y una lectura local; cuatro
referencias prestadas; dos condicionales y un wrapper desconocidos. La prueba
también rechazó la reconstrucción de uso único y conservó los hechos. Evidencia:
`A:/nsc-fixes-evidencia/async-yield-classification-expanded-20261112/`.
SHA-256 del ejecutable de prueba: `e31ef5d2441abf71a19d813be4e77e7c983a136b08d57a0d8f35e2773c71cea7`.
La compilación conjunta incluía además la infraestructura informativa de
orígenes inciertos de vistas; el guard de alias se había retirado previamente.
Esta evidencia no certifica el descarte durante cancelación.
Con el compilador de esa tanda, `cancel-drop-after-classification-20261112.log`
registró 18/18 controles de cancelación y abandono con salida 42. El runner de
namespace conservó sus resultados aprobados y los dos fallos SEH conocidos;
véase `namespace-after-classification-20261112.log`.

## Fixture de wrapper preparado, no ejecutado

`tests/unit/AsyncYieldOwnershipBindingTests.cpp` prepara un generador cuyo
finally hace `yield return InspectString("a" + "b")`, adaptando la operación
con `FullExpressionCleanup` y `OwnedTemporaryBorrow` demostrada en
`Stage50_8OwnershipIrLoweringTests.cpp`. El objetivo es observar ambos nodos
en el operando de yield y marcar como `UNKNOWN` cualquier wrapper sin binding;
observar el wrapper no acredita que su ownership esté resuelto. Esta ampliación
está pendiente de compilación y ejecución dirigida y no demuestra aún ninguna
forma real hasta que esa prueba termine.

Ejecución posterior: el target incremental compiló y la prueba terminó con
salida 0 (`A:/nsc-fixes-evidencia/async-yield-wrapper-build-20261112.log` y
`async-yield-wrapper-20261112.log`). En ese fixture se observó un
`FullExpressionCleanup` asociado a binding `builtin:int` y un
`OwnedTemporaryBorrow` asociado a `builtin:string`; ambos tenían binding, por
lo que el conteo Unknown del wrapper fue 0. El control sintético sin sintaxis
se registró como `UNKNOWN`, y el poll de la prueba de caché se restauró. El hash
del ejecutable de prueba fue
`3c810bed78925228ae6d66135cea3404e09e41e4a0c64b9b51f817ce308ffcf2`.
Esto comprueba la forma observada y la consulta, no demuestra que el binding
del wrapper describa el ownership del hijo ni resuelve descarte, excepción o
suspensión.

## Propuesta: transporte conservador de clasificación compuesta

Estado: implementación informativa compilada y verificada con las pruebas
dirigidas descritas abajo. No activa el descarte ni modifica LLVM.

Para una selección, comprobar primero que el MIR tiene exactamente tres hijos
no nulos, que la condición y su binding tienen tipo `builtin:bool`, y que el
resultado y su binding tienen el mismo tipo escalar incorporado. La condición
puede tener efectos: su tipo no demuestra pureza. Clasificar por separado ambas
ramas a partir de sus propios nodos y bindings; únicamente si ambas demuestran
`TrivialScalar`, mismo tipo que el resultado y ausencia de transferencia,
dueño fresco y ownership, transportar esa categoría al resultado. Un binding
ausente, conversión o productor no reconocido, tipo discordante o rama con
ownership conserva `Unknown`. En esta primera ampliación no transportar
`BorrowedReference`: unir referencias exige evidencia adicional de lifetime y
mutabilidad. Una condición SIMD no pasa la comprobación de bool escalar. El
backend actual evalúa ambas ramas para una máscara SIMD y solo la elegida para
bool; ninguna clasificación sustituye ni altera esos efectos.

Para `FullExpressionCleanup`, estudiar el transporte solo si existe un hijo
único, se demuestra el mismo tipo y categoría en el hijo y en el wrapper, y
se preserva la limpieza completa. Su binding compartido por sintaxis no prueba
por sí mismo que el hijo sea transparente. `OwnedTemporaryBorrow` conserva
`Unknown`: crea un dueño temporal y su limpieza no puede inferirse de la
categoría del resultado. Ninguna de estas reglas autoriza omitir evaluación,
efectos, excepciones o limpieza ni demuestra que sea seguro descartar un valor.

La implementación preparada usa una pasada iterativa posterior sobre los hechos
en orden inverso, con resultados internos por nodo y sin publicar categorías
de descendientes. Comprueba los índices `parentFact` y `childIndex`, exige tres
hijos completos y no mezcla yields. Antes
de cambiar codegen o usar estos hechos en él debe revisarse la identidad de la
caché de objetos pre-LLVM. La prueba dirigida preparada incorpora una selección
escalar con llamada en la condición y ramas int sin ownership; otra tiene una
llamada en una rama y debe seguir `Unknown`. El caso sintético sin binding de
una rama también debe quedar `Unknown`; se mantiene el control de wrapper y la
comparación entre caché fría, cálida y paralela. El control SIMD preparado es
sintético: cambia a `mask<builtin:int,4>` el tipo MIR de una condición cuyo
binding sigue siendo bool. Debe conservar `Unknown` ante esa inconsistencia; no
prueba la ejecución ni la semántica de una selección SIMD válida.

El fixture real anterior, con `AsyncIterator<simd<int, 4>, Exception>` y
selección por máscara, falló al construir MIR porque el iterador no tiene layout
físico: `async-iterator:simd<builtin:int,4>|type:Probe::NativeSharp.Core.Exception`.
El comando, configuración y salida están en
`A:/nsc-fixes-evidencia/async-yield-conditional-runner-20261112.log`; la fuente y
proyecto originales se conservan sin modificar en
`A:/nsc-fixes-evidencia/async-yield-simd-layout-repro-20261112/`. Este fallo
permanece pendiente de investigación; el control sintético no lo resuelve.
La compilación incremental del target y la ejecución dirigida posterior con el
control sintético pasaron: fría, cálida, binding desconocido y paralela
terminaron con salida 0; fría, cálida y paralela produjeron 61 registros
idénticos. La fría restauró 0 polls y la cálida 17. La selección escalar con
llamada en la condición se clasificó `TrivialScalar`; la selección con llamada
en una rama, el wrapper y el control de condición SIMD sintética quedaron
`Unknown`. Al retirar sintéticamente el binding de una rama, esa selección pasó
a `Unknown`. La prueba rechazó reconstruir el objeto de uso único y conservó
los hechos. Evidencia en
`A:/nsc-fixes-evidencia/async-yield-conditional-fixed-20261112/`; SHA-256 del
ejecutable de prueba:
`3e1b10a1d101435d41d55130582df1e2e34f76292eecef1072b3ea6e27b4a442`.
Esto verifica los hechos y su reconstrucción en caché, no ejecución de los
efectos de la condición, descarte ni selección SIMD válida.

Sondeo posterior de limpieza en ejecución: se prepararon por separado dos casos
con `Inspect(new Payload())`, uno normal y otro durante cancelación. Los cuatro
intentos O0/O2 se detuvieron en análisis semántico con `borrowed parameter
'value' requires a checked owner or borrowed source`. No hubo ejecución ni
evidencia sobre destrucción o limpieza para esas formas. Se conservan el script,
las fuentes generadas y el log bajo el prefijo
`A:/nsc-fixes-evidencia/async-yield-wrapper-cleanup-20261112`; esta observación
no demuestra un defecto de limpieza ni modifica el fixture original de rama
owned/prestada.

Un segundo sondeo usa `Make().Read()` como receptor temporal. Con el runtime y
las bibliotecas de importación aisladas en A, compiló en O0/O2. El caso normal
devolvió 42 en ambas configuraciones, tras comprobar una construcción, una
llamada y una destrucción antes del primer yield. El caso de finally cancelado
devolvió 22 en ambas porque `MoveNextAsync()` devolvió true tras `Cancel()`;
el test terminó antes de comprobar los contadores y por tanto no demuestra si
se limpió ese temporal en la ruta cancelada. El primer intento de este sondeo
falló por una ruta de runtime no configurada; el segundo usó las rutas del build
aislado. Se conservan fuentes, comandos y salidas bajo los prefijos
`A:/nsc-fixes-evidencia/async-yield-receiver-cleanup-20261112` y
`A:/nsc-fixes-evidencia/async-yield-receiver-cleanup-isolated-20261112`.
Un control posterior, conservado por separado en
`A:/nsc-fixes-evidencia/async-yield-receiver-counters-explicit-20261112`,
comprueba los contadores antes de salir si el avance devuelve true. O0/O2
devolvieron 65: el código lo hace solo con una construcción, una llamada y una
destrucción. Así, el receptor temporal se destruyó antes de ese yield inesperado
en este caso concreto; no se acredita la limpieza de una ruta de cancelación
corregida ni de excepciones. El resultado sigue incumpliendo la expectativa de
que el avance cancelado no produzca otro elemento.

### Controles de procedencia e identidad MIR

La prueba dirigida ampliada terminó con salida 0 en frío, cálido, binding
desconocido y paralelo; las tres ejecuciones comparables produjeron 61 registros
idénticos y restauraron 0 polls en frío y 17 en cálido. En cada fase comprobó
dos condicionales de propiedad mixta, uno en clase y otro en namespace: el
binding exterior no declara ownership ni transferencia, mientras una rama es
owned, fresca y transferida y la otra no lo es. Ambos continúan `Unknown`; el
binding exterior por sí solo no determina la propiedad de la rama ejecutada.

La misma prueba comprobó una identidad de limpieza de temporal: el ordinal de
región y el local de `OwnedTemporaryBorrow` coinciden con una región MIR
`TemporaryOwner` cuya acción apunta al mismo nodo. Esa comprobación pasó también
tras restaurar caché, y se conservó el rechazo de reconstrucción del modelo de
uso único. El control comprueba estructura e identidad; no acredita por sí solo
ejecución, orden ni limpieza ante excepciones. Los sondeos runtime anteriores
son evidencia separada y no justifican cambiar `Unknown` ni activar descarte.

Evidencia: `A:/nsc-fixes-evidencia/async-yield-owned-cleanup-20261112/`.
SHA-256 del ejecutable de prueba:
`b8a4ce1f5eef219ba510e2aa55d62541187f804b3feaab8f86c9e23fc39f3d7f`.

### Evidencia informativa por rama

Estado: implementación informativa compilada y verificada con los controles
dirigidos indicados abajo; no se consume en LLVM.
Cada hecho conserva `parentFact`, `childIndex` y su identidad de yield; sobre
ramas inmediatas de un condicional con condición y binding bool escalares se
añade una observación informativa. `SemanticFreshOwnedTransfer` requiere una
llamada estática con binding owned, fresh, transferido y propiedad owned tanto
del objeto como del almacenamiento; `StaticFieldNonOwning` requiere lectura
de campo estático con binding sin propiedad ni transferencia. Ambos requieren
binding y tipos MIR coincidentes con el condicional. Todo lo demás es `Unknown`.

Estos nombres describen hechos estáticos, no prueban lifetime ni autorizan
destrucción. La raíz mixta sigue `Unknown`; no se fusionan ramas, se reevalúa
la condición ni se omite cleanup. Los wrappers y sus regiones se conservan en
el árbol MIR; no se transporta propiedad a través de ellos. Antes de consumir
esto en codegen habrá que resolver qué resultado produjo cada ruta, mantener
los efectos y limpiezas de su expresión, comprobar excepciones y suspensión,
y revisar la identidad de caché de objetos pre-LLVM. Las pruebas preparadas
comparan evidencia entre procesos y cachés fría/cálida, ejecución paralela,
campos por rama, y binding de rama borrado sintéticamente.

El target incremental y el runner terminaron con salida 0 en frío, cálido,
binding desconocido y paralelo. Los tres resultados comparables contienen 61
registros idénticos; la ejecución cálida restauró 17 polls. Frío, cálido y
paralelo registraron dos ramas `SemanticFreshOwnedTransfer` y dos
`StaticFieldNonOwning`. Tras retirar sintéticamente el binding de una rama
owned, esa rama quedó `Unknown` y los contadores fueron una y dos,
respectivamente. El modelo rechazó su reconstrucción de uso único y conservó
los hechos. Evidencia:
`A:/nsc-fixes-evidencia/async-yield-branch-evidence-20261112/`; SHA-256 del
ejecutable:
`b7b64287765d026975d85a290d1329e5a4f4836ee422d62ed7997456daacdfe1`.
Estas pruebas demuestran las observaciones y su reconstrucción en caché, no la
corrección del descarte, la vida del alias ni cleanup ante excepciones.
