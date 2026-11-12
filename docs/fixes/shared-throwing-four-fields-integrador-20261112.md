# Construcción compartida fallida con campos compartidos

## Reproducción original

El fixture de `A:/nsc-vistas-evidencia/shared-four-fields-failure/Main.ns`
declara un payload de clase con campos `Rc`, `WeakRc`, `Arc` y `WeakArc`,
inicializa los cuatro y lanza. Exige capturar la excepción, destruir dos
payloads y no ejecutar el destructor del objeto incompleto. Su copia en
`A:/nsc-fixes-evidencia/shared-four-fields-integrador-20261112/Main.ns` es
idéntica según `fc /b`, SHA-256
`776a1d9f12aeea844dc213d2011f55219e3cba4e22759a671aac69cbc3ec78ac`.
El proyecto temporal cambia la referencia para consumir el Core binario de la
copia integradora; no modifica la fuente original.

Desde `A:/nsc-devel-fixes`, el script
`A:/nsc-fixes-evidencia/shared-four-fields-integrador-20261112/run.cmd`
configura temporales y runtime en A e invoca `--emit-project-oop-exe`,
`--no-artifact-cache`, O0 y O2. Las dos compilaciones devuelven 1 con
`shared throwing construction cannot safely clean this payload shape`.
Las salidas completas son `o0.log` y `o2.log` junto al script. No se ejecutaron
programas de esta reproducción.

El compilador aislado tuvo SHA-256
`223655d766ecd7309b67b08593ed2619da19c348da4fce894d9b83a042680545`;
`integracion-vistas-sdk/NativeSharpCore.lib` tuvo
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
su `.nsmeta` tuvo
`7d5921ca90d909c1e7fa98d092b9c0c28104dfe56f60f940e5deb89096203ccf`.
Los hashes coincidieron antes y después de la reproducción.

## Variantes de fábrica

El generador `A:/nsc-fixes-evidencia/shared-four-fields-integrador-20261112/variants.py`
conserva los programas, comandos y resultados en `variants/results.json`.
Se ejecutó mediante `variants.cmd`, con los mismos artefactos y entorno.

| Variante | O0 | O2 |
|---|---|---|
| Sin campos | compila 0, ejecuta 42 | compila 0, ejecuta 42 |
| Solo `Rc` | compilación 1 | compilación 1 |
| Solo `WeakRc` vacío | compilación 1 | compilación 1 |
| Solo `Arc` | compilación 1 | compilación 1 |
| Solo `WeakArc` vacío | compilación 1 | compilación 1 |
| Cuatro campos, throw tras inicializar uno | compilación 1 | compilación 1 |
| Cuatro campos, throw tras inicializar dos | compilación 1 | compilación 1 |
| Cuatro campos, throw tras inicializar tres | compilación 1 | compilación 1 |
| Cuatro campos, throw tras inicializar cuatro | compilación 1 | compilación 1 |

Las 16 compilaciones rechazadas informaron la misma barrera y no ejecutaron
programas; las dos del control sin campos sí comprobaron captura y ausencia del
destructor del objeto incompleto. Los hashes de los artefactos no cambiaron.
Las variantes rechazadas no permiten observar el orden de destructores ni la
liberación del bloque.

## Controles de construcción directa

`direct.py`, ejecutado mediante `direct.cmd` en la misma carpeta de evidencia,
usa `owned Failed value = new Failed()` y los cuatro campos. Sus cinco casos
lanzan antes de inicializar campos y después de cada inicialización; un sexto
termina normalmente. Cada caso compiló en O0 y O2 y devolvió 42: 12/12.
Exigen captura, destructor de `Failed` cero veces al fallar y una al terminar,
conteo de payloads y traza de destrucción inversa `21` si existen ambos. Las
fuentes y resultados se conservan en `direct/results.json`; los hashes de las
entradas no cambiaron. Esto no ejecuta la ruta de fábrica ni demuestra la
liberación física del bloque de esa fábrica.

### Movimiento y reemplazo en construcción directa

El 12 de noviembre se ejecutó `direct-transitions.cmd` desde la raíz de la
copia integradora. Su generador `direct_transitions.py` ejercita campos `Rc` y
`Arc` en cuatro escenarios: mover el campo a un local y lanzar; reemplazarlo y
lanzar; moverlo, reasignarlo y lanzar; y lanzar al construir el valor nuevo de
un reemplazo. En O0 y O2 las 16 combinaciones compilaron y devolvieron 42.
Además de exigir excepción capturada y cero destructores del objeto incompleto,
se comprobaron conteos de payload y traza `12` cuando el reemplazo destruye el
primer payload antes de que el unwind destruya el segundo. En el reemplazo que
lanza sólo se destruye el payload anterior, con traza `1`.

`direct-weak-transitions.cmd` ejecutó la misma selección para `WeakRc` y
`WeakArc`; también pasaron 16/16 en O0/O2. Cada caso comprueba que, tras el
fallo del constructor, el fuerte exterior sigue vivo, su conteo débil explícito
es cero y no se llamó al destructor del objeto incompleto; al salir del scope,
se destruye una vez el payload. Los punteros `unsafe` del fixture apuntan a un
fuerte local que permanece vivo y se usan dentro del mismo hilo.

Los dos scripts y los JSON con fuentes, comandos, códigos y hashes están en
`A:/nsc-fixes-evidencia/shared-four-fields-integrador-20261112/`, carpetas
`direct-transitions` y `direct-weak-transitions`. Los hashes de nsc, Core y
metadata coincidieron antes y después de cada selección. Las 32 ejecuciones
validan esos controles directos concretos; no prueban la construcción mediante
fábrica, condiciones concurrentes, destrucción física del bloque ni todo el
comportamiento frente a excepciones durante destructores.

## Análisis y trabajo pendiente

`src/LLVM/Lowering/OopLlvmConstruction.cpp` rechaza campos que requieren
destrucción de valor en la construcción compartida que lanza; la barrera
posterior produce el diagnóstico observado. La inspección y las cuatro
variantes individuales son consistentes con esa condición. La rama no se
instrumentó, así que no se afirma haber observado su decisión interna.

Hay infraestructura existente: `OopLlvmStatements.cpp` reconoce un estado
vacío comprobado para las cuatro familias y registra campos y banderas de vida;
`OopLlvmCleanup.cpp` emite la destrucción de valor cuando la bandera está viva.
`SharedControlStorage.CreateThrowing` libera el bloque en `finally` si no se
transfiere. La ruta placement también posee una limpieza de respaldo para
punteros `owned`, que no equivale a limpieza general de valores. Estas piezas
por sí solas no prueban seguridad conjunta para la fábrica binaria.

Una vía por investigar es verificar si el constructor ya limpia cada campo y
placement puede confiar en ello sin un segundo drop. Antes de modificar la
barrera debe demostrarse el reconocimiento del estado vacío con metadata
binaria; banderas antes de cualquier excepción; asignación, reemplazo y move;
orden inverso y bases; ausencia de destrucción doble entre constructor y
placement; propagación de la excepción y liberación única del bloque. El
tratamiento de reemplazo en `OopLlvmArraysAndStores.cpp` debe revisarse frente
a los dos identificadores de acciones de limpieza. No se permite extender la
aceptación a otros valores o layouts sin demostrar su vida parcial.

La validación futura debe cubrir el original, cada punto de fallo, valores
vacíos y movidos, reemplazo que falla, orden, constructores anidados y bases,
ambas fábricas, clases y structs, y Core binario en O0/O2. La liberación física
requiere evidencia propia. No se modificó código ni se retiró la barrera.

## Factibilidad de placement y descarte doble

La ruta `initializePlacementStorage` inicializa el almacenamiento a cero antes
del constructor cuando `node->zeroInitialize` está activo; la propia barrera
exige esa condición. El constructor registra acciones de limpieza de campos y,
para el estado vacío de shared que logra reconocer, inicia su bandera viva.
`emitCleanupAction` desactiva la bandera antes de destruir el valor por su
dirección. Si el constructor retorna normalmente, `lowerBlockStatement` retira
esas acciones sin ejecutarlas. En fallo, `CreateThrowing` mantiene la propiedad
del arreglo y lo libera en `finally`. Los controles directos anteriores
respaldan esta cadena para el caso concreto sin placement.

Una solución candidata estrecha sería compartir entre la clasificación de
placement y la preparación de cleanup del constructor la misma comprobación
estructural del estado vacío, y permitir en placement únicamente campos de esas
cuatro familias cuya prueba sea positiva. La limpieza de cada valor quedaría
exclusivamente en el constructor; placement no llamaría otro drop para esos
campos. Los punteros `owned` conservarían su camino existente. Cualquier tipo,
base, layout o procedencia no demostrada seguiría sujeto a la barrera. Esto es
un diseño propuesto: no se ha aplicado ni comprobado en la fábrica.

La comprobación actual es una lambda local a `lowerBlockStatement` y requiere
un layout no importado y un destructor fuente con guarda de bloque nulo. Antes
de extraerla o reutilizarla hay que demostrar que el constructor y la
especialización del placement del Core binario ven la misma identidad, layout y
cuerpo; el nombre genérico por sí solo no prueba el estado vacío. También es
preciso comprobar que las dos rutas no divergen si hay herencia o solapamiento.

Los movimientos de campos en `OopLlvmExpressionMachine.cpp` sólo ponen el slot
a cero cuando el MIR indica `clearSourceStorage`; su cambio de bandera busca la
acción `constructor.owned.field.cleanup`, no la acción shared. El reemplazo en
`OopLlvmArraysAndStores.cpp` también busca la acción owned para consultar su
bandera. Con el estado shared inicialmente vivo, esto puede ser coherente si
cada movimiento deja el valor vacío y cada reemplazo conserva un estado válido
ante una excepción, pero todavía no se ha demostrado. En particular,
`emitValueDropAddress` no demuestra por sí sola que el almacenamiento se ponga a
cero tras destruirlo: añadir un drop de placement sobre el mismo slot podría
destruir dos veces. No se debe tratar la existencia del control directo como
prueba de estas transiciones ni ampliar la barrera hasta verificar los casos.

Una inspección adicional acota el riesgo de move ordinario: en
`src/Frontend/Bodies/ExpressionCore/MoveExpressions.cpp`, el binding de un campo
marca `clearSourceStorage` cuando `typeRequiresValueDrop` es verdadero. HIR
propaga esa bandera y el backend limpia el slot si la recibe. Esto es evidencia
estática para esa ruta; no sustituye un control de ejecución de las variantes
de fábrica ni demuestra caminos que no utilicen ese binding.

`docs/fixes/prestamo-get-punteros.md` registra además pruebas históricas de
constructores de contenedor y Core binarios: en un build anterior se corrigió la
identidad del estado vacío y pasaron 32 combinaciones y 16 controles de
homónimos. En esa inspección los layouts de la reproducción no estaban marcados
como importados. Es evidencia útil de la comprobación estructural, pero usa
otros artefactos y no valida la fábrica ni la liberación física del bloque en
la revisión actual.

## Control parcial de delegación directa

Se inició `A:/nsc-fixes-evidencia/shared-four-fields-integrador-20261112/direct-delegacion.cmd`
desde la copia integradora. `direct_delegacion.py` crea un constructor sin
argumentos que delega con `this(1)` y otro que inicializa dos campos fuertes.
Sus variantes esperan fallar tras la primera o segunda inicialización, fallar
en el cuerpo exterior, o terminar normalmente; comprueban conteos y traza de
IDs de payload. No emplean la fábrica como constructor exterior.

Las ocho ejecuciones de `Rc` y las dos de `Arc` con fallo tras el primer campo
compilaron en O0/O2 y devolvieron 42. El siguiente caso,
`Arc-inner-second/O0.exe`, compiló y no terminó durante los 30 segundos fijados
por el runner; Python lo terminó y lanzó `subprocess.TimeoutExpired`. El runner
salió con código 1. La duración excedida no demuestra una doble destrucción,
un deadlock ni su causa. No se repitió esa ejecución.

`direct-delegacion/results.json` es **parcial**: contiene solamente los diez
casos terminados y los hashes iniciales; no contiene el caso que excedió el
tiempo, los cinco casos siguientes ni hashes finales porque el runner interrumpió
antes de registrarlos. La fuente y ejecutable del caso interrumpido permanecen
en `direct-delegacion/Arc-inner-second/`. La salida parcial fue:

```text
Rc-inner-first O0/O2: compile 0, run 42
Rc-inner-second O0/O2: compile 0, run 42
Rc-outer O0/O2: compile 0, run 42
Rc-success O0/O2: compile 0, run 42
Arc-inner-first O0/O2: compile 0, run 42
Arc-inner-second O0: compile 0, ejecución excedió 30 segundos
```

El compilador tenía al comienzo el SHA-256
`a2ed817580814531c63f0a117a5e875a3eeb151362925fb7b117f2b6da201675`; el hash
consultado después coincide. Core y metadata también coinciden con los hashes
del registro inicial. La lectura posterior comprueba el estado en esos momentos;
no demuestra por sí sola que no hubiera sustituciones intermedias. Los cinco
casos no iniciados quedan sin resultado, y la selección no constituye éxito.

Estáticamente, `prependConstructorWork` antepone la llamada de inicializador y
omite los inicializadores del tipo exterior en delegación `this(...)`; el
constructor terminal los ejecuta. En `lowerBlockStatement`, `prepareFields`
se prepara antes del primer hijo que no es llamada de base, mientras que una
llamada delegada no lleva `baseReceiver`. Así, el exterior puede registrar su
propia limpieza de campos antes de llamar al constructor terminal. El terminal
también registra la suya; la posible interacción durante el unwind y su relación
con el timeout aún no están demostradas. La barrera de fábrica permanece intacta.
