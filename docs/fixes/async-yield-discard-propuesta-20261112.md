# Propuesta: propiedad del resultado de yield al cancelar

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
Tampoco se han
verificado workers paralelos, cambios de grafo ni invalidación de identidad de
objeto. Los hechos registrados son evidencia de binding, no una clasificación
de propiedad ni una solución del descarte.

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
