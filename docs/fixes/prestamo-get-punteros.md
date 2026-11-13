# Acceso prestado a los controles compartidos

Con el compilador incremental posterior al ajuste provisional de construcción,
las selecciones dirigidas de límites y contienda de contadores pasaron 2/2:
los límites de Rc/WeakRc y Arc/WeakArc ejecutaron 42 en O0/O2, y las contiendas
de retención fuerte y débil de Arc ejecutaron 42 en O0/O2
(`A:/nsc-fixes-evidencia/shared-counter-directed-latest.log`). Los fixtures
preparan el contador internamente con `unsafe` para llegar al límite; la prueba
no demuestra que Arc y WeakArc ya satisfagan el contrato público Send/Sync ni
resuelve la fuga durante la construcción documentada más abajo.

Tras el cambio provisional de limpieza de constructores se repitió una
selección dirigida con las fuentes públicas y el compilador incremental de A:
para Rc y Arc se rechazaron diez formas inseguras por familia, incluidas
escritura, escape, movimiento o reemplazo durante el préstamo y un cruce con
`await`. Los tres controles positivos por familia ejecutaron 42 en O0 y O2:
12 ejecuciones y 20 rechazos (`A:/nsc-fixes-evidencia/readonly-owner-directed-current.log`).
La prueba no cubre consumo binario ni la llamada de reemplazo mediante método,
cuya decisión de contrato sigue pendiente más abajo.

Una prueba nueva usa `WeakRc` o `WeakArc` como campo de un objeto cuya
construcción falla después de que un método recibe y mueve allí el débil. En
Rc y Arc, O0 y O2, la excepción se captura pero el conteo débil permanece en
uno; se esperaba cero (`A:/nsc-fixes-evidencia/weak-helper-matrix-before.log`,
salida 11 frente a 42). La regresión
`weak_field_helper_constructor_failure` conserva esa expectativa y falla.
Como contraste, asignar el débil directamente en el constructor antes de
fallar, o asignarlo por el método y completar la construcción, ejecutó 42 para
ambas familias y ambos niveles (`weak-helper-positive-controls.log`, ocho
ejecuciones). El prototipo de cleanup rastrea stores en el cuerpo del
constructor, pero el store de esta reproducción está en otro método; no se ha
demostrado todavía una solución segura para marcar el comienzo de vida de un
campo de valor indirecto, en particular si ese método también puede lanzar.
No se alteró el compilador en esta comprobación.

La variante `weak_field_helper_itself_failure` hace que el método lance después
de mover el débil al campo. Rc y Arc vuelven a devolver 11 en O0 y O2 frente a
42 (`A:/nsc-fixes-evidencia/weak-helper-inner-throw.log`). Por ello, marcar el
campo únicamente cuando regresa la llamada no corrige la salida excepcional
del método. El MIR de la reproducción inicial conserva la llamada de instancia
en el constructor y el `store` del campo en el cuerpo de `Initialize`, con
ordinals distintos (`A:/nsc-fixes-evidencia/weak-helper-mir/mir.log`). El
almacenamiento del objeto mostrado en ese MIR se inicializa a cero, pero esa
observación no establece el inicio de vida para todos los campos de valor ni
autoriza destruir indiscriminadamente valores que aún no se construyeron. La
segunda regresión conserva la expectativa de éxito y sigue fallida.

Los controles se ampliaron con un débil movido fuera del campo por otro método
antes de la excepción y con un campo débil que nunca recibe un valor. Ambos
ejecutaron 42 para Rc y Arc en O0/O2, junto con los dos controles anteriores:
16 ejecuciones (`A:/nsc-fixes-evidencia/weak-helper-positive-controls.log`).
El catálogo MIR conserva una clave de especialización como
`Probe::NativeSharp.Core.WeakRc` con su argumento y las funciones asociadas;
la función también registra el ordinal de su tipo declarante
(`weak-helper-mir/mir.log`). Es una posible fuente para identificar el tipo
sin inferirlo solo por el nombre generado, pero no se ha verificado que exista
para todas las bibliotecas precompiladas o especializaciones sin métodos
materializados. En ese momento se consultó al usuario el contrato específico
del estado vacío de las cuatro familias.

El usuario aprobó después que el estado predeterminado de `Rc`, `WeakRc`,
`Arc` y `WeakArc` sea vacío y pueda destruirse de manera segura. El ensayo del
backend registra los campos directos de esas familias para cleanup desde el
estado vacío, de modo que cubre la asignación por un método incluso cuando este
lanza. Distingue la acción de cleanup para que mover el campo a vacío no pierda
el seguimiento de una reinicialización posterior. Para identificar el tipo usa
su layout y ordinal, el namespace y la clave de especialización de una función
MIR del mismo tipo; no aplica la regla a uniones, offsets explícitos ni tipos
importados. Si falta una función MIR que permita reconocer la especialización,
no se acredita que el ensayo cubra ese caso. No cambia la regla de estructuras
ajenas ni implementa seguimiento general de vida por campo.

Con el compilador incremental de hash `e45d33bc3c581d15`, las dos
reproducciones débiles anteriormente fallidas pasaron para Rc y Arc en O0/O2
(`weak-helper-matrix-after.log`, `weak-helper-inner-after.log`). La selección
ampliada comprobó además campos fuertes, campo vacío, asignación directa,
movimiento, reinicialización mediante método y un `Other.WeakRc` ajeno: 48
ejecuciones con salida 42 (`shared-field-default-controls.log`). Las pruebas de
estructura y construcción existentes pasaron otras 20 y 68 ejecuciones,
respectivamente (`struct-owned-helper-controls.log` y
`shared-field-default-regression.log`); todos los logs están en
`A:/nsc-fixes-evidencia`. Los controles verifican conteos y destructores, no la
liberación directa del bloque. El SDK binario, campos anidados y tipos sin
especialización MIR reconocible siguen pendientes; el ensayo no tiene commit.

El usuario eligió que `Rc.Get()` y `Arc.Get()` devuelvan una referencia
prestada de solo lectura ligada al propietario, tanto para clases como para
estructuras. El acceso directo se añadió primero a los controles **internos**
local y atómico. Hay una implementación pública inicial en el árbol de trabajo,
pero todavía no está disponible como SDK binario ni acredita todos los contratos.

El prototipo mostró que `Payload escaped = owner.Get()` podía copiar una
referencia de clase y usarla después de destruir el propietario. El análisis
de llamadas trata ahora como prestado el resultado de clase obtenido mediante
un retorno de referencia de instancia que no declara vida estática. Para un
préstamo local tomado de una referencia que depende de `this`, el compilador
registra como origen al receptor local con movimiento exclusivo. Las llamadas
con vida estática explícita mantienen su contrato y las que declaran otra
vida no se vinculan arbitrariamente al receptor.

Las pruebas dirigidas `nsc.sdk.shared-internal-handles` comprueban para los
controles local y atómico el acceso a clases y estructuras en O0/O2, la
lectura mediante alias prestado, el rechazo de escritura en el valor de una
estructura, la prohibición de mover al propietario mientras vive el préstamo
y el rechazo de escapes por local, campo, retorno y argumento. También pasan
las otras cuatro pruebas de controles y almacenamiento compartido, la
comprobación semántica de Core y las pruebas dirigidas de vistas y seguridad de
hilos (`shared-control-suite.log`, `shared-core-check.log` y
`nested-thread-safety-tests.log` en `A:/nsc-fixes-evidencia`).

Esta evidencia no demuestra aún la seguridad de todas las formas de receptor,
las referencias cruzando módulos precompilados ni la transferencia segura
de `Arc` entre hilos. La API pública sigue pendiente hasta acreditar el pin
del módulo productor y el contrato `Send`/`Sync` apropiado. `WeakCount()`
atómico tendrá el contrato elegido: aproximado ante concurrencia y exacto
cuando no hay operaciones simultáneas.

La prueba dirigida `strong_empty_lifecycle` verifica que clonar un `Rc` o
`Arc` predeterminado y hacer downgrade/upgrade conserva referencias vacías,
contadores cero y ninguna destrucción de payload. Pasó para ambas familias en
O0/O2 junto con el control de débil vacío; evidencia
`A:/nsc-fixes-evidencia/shared-empty-lifecycle.log`. No prueba overflow ni
seguridad de acceso a un payload inexistente.

Una comprobación posterior encontró que escribir `owner.Get().Data` es
rechazado, pero `borrowed Payload alias = owner.Get(); alias.Data = 100l;`
es aceptado (`A:/nsc-fixes-evidencia/borrowed-class-mutation-probe.log`).
Por separado, una clase con campo público mutable satisface actualmente una
restricción `Sync` (`probe-sync-mutable-class/results.json`). Estas dos
observaciones no son una prueba de seguridad de `Arc`. El usuario eligió que
el alias conserve la prohibición de escritura.

## Propagación del préstamo de solo lectura

El análisis semántico conserva la restricción en variables prestadas, campos
de referencia anidados, conversiones de referencia, expresiones condicionales
y `??`. Rechaza una reasignación que intentaría introducir el préstamo de solo
lectura en una variable prestada mutable, para evitar perder la restricción en
una unión de flujo o en una vuelta de un ciclo. También impide retornar ese
objeto mediante un contrato prestado mutable.

Como los métodos de clase, getters y parámetros actuales no acreditan un
contrato de receptor u objeto inmutable, las llamadas correspondientes, la
conversión a delegado, el pase a argumentos y los constructores se rechazan
conservadoramente en los casos comprobados. Esta restricción puede impedir
operaciones que serían seguras; no demuestra que exista una API general de
parámetros de clase de solo lectura. El acceso a campos escalares se mantiene.

En la copia y build aislados de A, con el compilador autorizado y los
temporales en A, las comprobaciones dirigidas registraron:

- `shared-control-suite.py`: 5/5, con los controles internos local y atómico,
  lecturas y accesos vacíos en O0/O2 y rechazos de escritura por alias,
  cadenas, campos anidados, conversiones, condicionales, métodos, propiedades,
  indexadores, delegados, parámetros, constructores y retorno mutable.
- `nested-thread-safety-tests.py`: 2/2; `run-reference-unit-tests.py`: 2/2.
- `clang-tidy --checks=clang-analyzer-*` en los diez archivos C++ modificados y
  una segunda pasada en los archivos modificados después del primer análisis:
  salida 0. Los 11 registros de la primera serie contienen 55 avisos de padding
  de encabezados, sin otros tipos de warning observados en esa serie. Las
  salidas están en `A:/nsc-fixes-evidencia/readonly-borrow-analysis-*.log` y
  `readonly-borrow-final-*.log`; el resumen cuenta avisos repetidos por unidad.

La evidencia no cubre todas las formas posibles de conversión, captura,
metadata precompilada ni todas las combinaciones de flujo. La garantía de
`Send`/`Sync`, la vida del módulo productor y la distribución binaria de las
cuatro API públicas siguen pendientes. No se ejecutó una batería completa,
por instrucción del usuario.

Una comprobación dirigida adicional intentó guardar el alias de `Get()` en una
propiedad y en un indexador de usuario. Ambas asignaciones se rechazaron para
`Rc` y `Arc` desde fuentes, con el diagnóstico específico de setter que no
demuestra retención. En Rc se inspeccionó además la ubicación del diagnóstico
sobre el alias; los resultados están en
`public-tuple-borrow-property_assignment_escape,indexer_assignment_escape.log`.
Esto no verifica todas las implementaciones posibles de setters ni el SDK
binario.

## Excepción dentro del destructor y campos propietarios

Tras `a60eb0b64`, una reproducción con un destructor que llama a un método cuyo
inicializador estático lanza comprueba también un campo `owned Resource` del
payload. En las ocho combinaciones de control local/atómico, clase/struct y
O0/O2 se captura la excepción y se entra una vez al destructor, pero el
destructor del campo no corre: salida 13, esperada 42. Fuentes y resultados en
`A:/nsc-fixes-evidencia/destructor-owned-field-exception/*-before.log`.

La inspección identifica `emitObjectDestructionSteps` para clases y
`emitStructValueDropMembers` para structs: ambos emiten el cuerpo del destructor
antes de los campos, sin una ruta que continúe con ellos si ese cuerpo
propaga una excepción. Son rutas compartidas con la destrucción ordinaria; no se
han modificado. Se consultó el contrato para completar la limpieza, propagar la
primera excepción y tratar una segunda durante el unwind. La decisión sigue
pendiente; la prueba no confirma qué sucede con bases ni con un segundo fallo.

Una reproducción independiente con un propietario ordinario, sin las familias
compartidas, confirma el mismo orden: en O0/O2 se captura la excepción y se
observa el destructor del payload, pero no el del campo (`salida 4`;
`A:/nsc-fixes-evidencia/destructor-release-probe/results.log`). El IR del mismo
proyecto (`llvm.log`) coloca la liberación del campo después del cuerpo del
destructor y la llamada a `__nativesharp_free` después de la destrucción del
objeto, sin una ruta local de cleanup para una salida excepcional. Esto muestra
que esas llamadas se omiten en esa ruta del IR; no se midió la memoria en tiempo
de ejecución ni se probaron bases o una segunda excepción. La solución y el
contrato de propagación siguen pendientes de la decisión ya consultada.

Reproducción posterior con el compilador aislado de build hash
`c0611dacbb48e5c9`: `destructor-release-current.cmd` copia los dos archivos de
la reproducción a un temporal de A, compila sin caché y ejecuta O0 y O2. En
ambos casos vuelve a salir 4: se observan el destructor exterior y la captura,
pero no el destructor del campo. La prueba
`tests/integration/destructor_throw_owned_field.py` conserva la expectativa
42 y su fixture contiene copias intactas de la reproducción; sigue fallando y
no se ha registrado como aprobada. El log es
`A:/nsc-fixes-evidencia/destructor-release-current.log`.

En el generador actual, `emitObjectDestructionSteps` emite el cuerpo del
destructor antes de la liberación de campos y `emitReleasingDestructionEntry`
llama a la liberación del bloque solo después de que retorne esa secuencia.
Ninguna de esas dos rutas instala una continuación excepcional local para
completar esos pasos. El cambio debe conservar el primer error mientras limpia
los campos y libera el bloque, y definir y probar el caso de una segunda
excepción durante esa limpieza; no se ha cambiado el generador.

La regresión se amplió con un payload que destruye dos campos en orden inverso:
el destructor del segundo lanza y el del primero debe ejecutarse después. En
O0 y O2 la excepción se capturó, pero la traza fue `32` en vez de `321`: se
ejecutaron el destructor exterior y el del segundo campo, sin llegar al primero.
Un control sin excepción obtuvo `31` y terminó 42 en ambas configuraciones.
La reproducción original sigue devolviendo 4; los seis resultados constan en
`destructor-release-current.log` y el resultado anterior separado permanece en
`destructor-release-original-current.log`. Los controles no miden liberación
física ni cubren bases, campos de valor, ni una segunda excepción durante el
desenrollado; el arreglo sigue pendiente.

Se ensayó una cadena de limpiezas excepcionales para los pasos restantes y la
liberación del bloque. El ensayo hizo pasar en O0/O2 la excepción exterior, la
del campo, la salida normal, la limpieza de la base y la de un campo de valor
(`destructor-release-prototype-single.log`); también pasó la selección dirigida
de Rc y Arc `destructor-shared-regression.log`. Sin embargo, con dos
excepciones durante la misma destrucción se propagó la segunda y solo se
observó la destrucción de una de las dos: salida 51 en O0/O2
(`destructor-double-throw/results.log`). El contador demuestra las destrucciones
observadas en este fixture, no una medición directa del heap. No se declara una
política general para sustituir excepciones ni un resultado seguro.

El ensayo se conservó en `destructor-release-prototype.patch` y se retiró del
código. La compilación incremental restaurada terminó correctamente; su nsc
tiene SHA-256 `ef1417f5d1d25c98af26ecc8e177aae96e7de19a805c32c4d0db19f0753faf34`.
La selección restaurada vuelve a fallar los cuatro escenarios excepcionales y
pasa el control normal en O0/O2 (`destructor-release-current.log`). La siguiente
solución debe dar una propiedad explícita a la excepción desplazada, comprobar
el caso de errores externos a NativeSharp y preservar las limpiezas restantes.

Se eligió como contrato propuesto terminar mediante trap identificable si una
segunda excepción sale de un destructor mientras se desenrolla la primera; una
sola excepción debe seguir propagándose después de completar la limpieza. El
primer ensayo de ese contrato colocó un `catchswitch` dentro del `cleanuppad`.
LLVM rechazó la generación O0 y O2 con `Cleanup funclets for the SEH personality
cannot contain exceptional actions` (`destructor-double-throw/results.log`);
ningún ejecutable de esa variante llegó a ejecutarse. El script exploratorio
devolvió inicialmente éxito pese a los fallos de compilación; se corrigió para
devolver fallo en ese caso. Se retiró la variante y se restauró incrementalmente
el compilador SHA-256
`b5297b7b3ef4c84fd3ee34be21463d8e928459e52724550d0419616c767517c1`.
Los controles restaurados siguen mostrando el fallo original. Una posible
alternativa es un helper con frontera excepcional propia llamado desde el
funclet, pero aún no se ha implementado ni demostrado su validez SEH o su
tratamiento de excepciones externas. El contrato sigue sin implementación.

El experimento aislado `A:/nsc-fixes-evidencia/seh-double-helper/` usa LLVM IR
escrito a mano y Clang de referencia para invocar una función con su propio
`catchswitch` desde un `cleanuppad`. La salida fue 42 si solo ocurre la primera
excepción y 148 si la función auxiliar genera una segunda y su handler termina
el proceso. `build.log` registra una advertencia de sustitución del triple y
`results.log` conserva ambas salidas. El control utiliza excepciones software
de Windows y `ExitProcess`; no es código emitido por NativeSharp y no demuestra
el comportamiento ante toda excepción extranjera ni la liberación de recursos.

La regresión del compilador incluye ahora `Double.ns` y exige salida 148 en
O0/O2 para la decisión de trap. Con el compilador restaurado obtuvo 21 en ambos
casos: se capturó la primera excepción antes de ejecutar el campo que lanzaría
la segunda. La prueba completa sigue fallando para las rutas excepcionales
(`destructor-release-current.log`); el control normal pasa. El experimento de
LLVM hace viable investigar el helper, pero la integración todavía está
pendiente y no se ha certificado la solución.

La integración provisional genera la cadena de acciones restantes y ejecuta
cada acción excepcional mediante un helper con su propia frontera SEH. Una
sola excepción continúa después de la limpieza; si una acción del helper lanza
una segunda, el helper termina el proceso con el código 148. La entrada que
posee el almacenamiento ejecuta su liberación tanto al retornar como al
desenrollar. Este contrato de segunda excepción es una decisión de la campaña:
no garantiza que las acciones restantes se ejecuten después del trap.

Con el compilador aislado SHA-256
`a87a732452050b43c68e27420e9e49fdf6b25f674deb7776e3e33edab04e0647`,
`nsc.compiler.destructor-throw-owned-field` pasó 1/1 con 14 ejecuciones:
excepción del destructor exterior, excepción de un campo, salida normal,
base, campo de valor y doble excepción en O0/O2, más controles normal y doble
con depuración en O0. Los casos simples devolvieron 42 y los dobles 148
(`destructor-release-registered.log`). Una selección de ocho casos de
construcción y limpieza para cada familia Rc/Arc pasó en O0/O2
(`destructor-shared-regression.log`); también pasó la selección de vistas desde
fuentes ejecutada antes del último ajuste. No se ejecutó batería completa.

La evidencia corresponde a Windows x64 y al árbol conjunto sin commit. No mide
directamente la liberación física, no cubre todas las excepciones ajenas a
NativeSharp, asignadores personalizados, todas las formas de clases importadas
ni el coste de generación de helpers. Los cambios siguen provisionales hasta
resolver esos límites pertinentes y separar un commit verificable.

La reproducción `shared_destructor_throw_fields.py` volvió a ejercitar los
controles de cuenta local y atómica con payload clase y struct desde las fuentes
actuales de Core. Las cuatro combinaciones de clase devolvieron 42 en O0/O2;
las cuatro de struct devolvieron 13 porque no se destruyó el campo propietario
(`shared-destructor-throw-current.log`). El ejecutor crea un proyecto temporal
en A y no modifica las reproducciones originales. La corrección de clases no
se extiende todavía a `emitStructValueDropMembers`; la limpieza de struct sigue
pendiente y la selección completa falla deliberadamente.

El ensayo posterior construye un plan de destrucción para la entrada independiente
de struct y reutiliza la cadena de limpieza excepcional. Conserva el tratamiento
especial de arreglos, cadenas y delegates propietarios; una llamada al drop glue
con destino excepcional se emite como llamada potencialmente lanzadora. La ruta
inline ya dentro de un funclet o de una región de limpieza conserva su lógica
anterior y todavía no queda acreditada por esta modificación.

Con el compilador SHA-256
`b749ab54020017a313cb0db9ba74142c52e8d6b39909129528991b4e2e68b1bf`,
la prueba `nsc.sdk.shared-destructor-throw-fields` pasó 16 ejecuciones: control
local y atómico, payload clase y struct, O0/O2, tanto con una excepción como
con una segunda durante la limpieza. Las primeras devolvieron 42 y las segundas
148. La selección registrada junto a la regresión de destrucción ordinaria pasó
2/2 (`shared-destructor-registered.log`); la selección existente de tuplas y
consumo binario de valores pasó 2/2 (`struct-drop-regression.log`). Los controles
no miden directamente la liberación del bloque ni cubren todas las rutas inline,
arreglos de valores, excepciones extranjeras o asignadores. La implementación y
las pruebas siguen en el árbol conjunto sin commit.

El control `fixed-array-element-throws` reprodujo otra omisión: un arreglo fijo
de dos valores destruía el elemento 2, cuya excepción se capturaba, pero no
destruía el elemento 1; la traza fue `2` en O0/O2. El ensayo posterior construye
la cadena de limpieza en orden inverso para arreglos fijos fuera de funclets y
conserva el destino excepcional exterior. Con el compilador SHA-256
`e57744a6f17a832e5db8e9d06bf25e48a570f85c77db70170e85aab553ba1860`,
la regresión pasó el caso inicial, el campo propietario posterior de una clase,
el campo posterior de un struct y el trap de segunda excepción en O0/O2
(`destructor-release-registered.log`). La selección compartida de 16 casos y
las dos pruebas de tuplas y valores binarios volvieron a pasar
(`shared-destructor-throw-current.log`, `struct-drop-regression.log`). Esto cubre
arreglos fijos en esas formas, no arreglos dinámicos ni ejecución inline dentro
de un funclet o región activa; tampoco mide liberación física.

El control `dynamic-array-element-throws` reproduce el caso equivalente para
`owned Part[]`: el elemento 2 lanza, la excepción se captura, pero la traza es
`2` en vez de `21` tanto en O0 como en O2. La regresión registrada vuelve a
fallar por este caso (`destructor-release-registered.log`). La fuente del
recorrido usa un índice dinámico y salta a la liberación del bloque solo después
de completar el bucle; una excepción interrumpe ambas operaciones. Se necesita
conservar la posición pendiente, evitar repetir el elemento que ya empezó a
destruirse, limpiar el resto y liberar el bloque antes de propagar. Una segunda
excepción debe seguir el contrato de trap elegido; el diseño todavía no está
implementado ni se ha medido la liberación física.

El ensayo posterior marca el elemento como no vivo antes de invocar su drop y,
cuando esa llamada lanza, recorre los índices restantes en orden inverso,
omitirá los elementos no vivos y libera el bloque antes de continuar el
desenrollado. Las llamadas durante esa limpieza pasan por un helper con frontera
SEH propia: si una segunda excepción escapa, termina con el código 148. Esta
implementación se aplica a elementos de valor con drop glue disponible fuera de
funclets y regiones de limpieza activas; las demás rutas conservan su lógica
anterior.

Con el compilador SHA-256
`f592f9c7f7c67926791dc40254df204bea9d0a66f1ea8e30b25f1799ac71a76f`,
la regresión registrada pasó la reproducción dinámica en O0/O2 y los controles
de un elemento nunca inicializado, un campo exterior pendiente y una segunda
excepción (`destructor-release-registered.log`). También pasaron la selección
de 16 casos compartidos y las pruebas dirigidas de tuplas y valores binarios
(`shared-destructor-throw-current.log`, `struct-drop-regression.log`). La
evidencia comprueba trazas y códigos de salida; no mide directamente el heap ni
acredita arreglos de referencias, dimensiones adicionales, asignadores o las
rutas que no entran en la nueva condición. El ensayo sigue sin commit.

La prueba `dynamic-reference-array-throws` confirmó que un arreglo dinámico de
clases detenía su recorrido tras el primer destructor que lanzaba: traza `1` en
vez de `12`, en O0/O2. El ensayo continúa ahora desde el siguiente índice en el
orden previo de esa ruta, limpia cada slot antes de destruir su objeto y libera
el bloque antes de propagar; otra excepción durante la limpieza termina con 148.
Con el compilador SHA-256
`392aa7ff1e8a85459f776351bd798a39e33b646501f31cee1428442df4a1fd7d`,
la regresión pasó O0/O2 para el caso original, un slot nulo con campo exterior
pendiente y una doble excepción (`destructor-release-registered.log`); los dos
últimos también pasaron O0 con información de depuración. También
pasaron los controles dirigidos de vistas indexadas, destrucción compartida,
tuplas y valores binarios (`readonly-indexed-prototype2.log`,
`shared-destructor-throw-current.log`, `struct-drop-regression.log`). La ruta
nueva se limita a elementos de clase fuera de funclets y regiones de limpieza
activas; no mide directamente el heap ni acredita otras categorías de elementos,
dimensiones o asignadores. Sigue sin commit.

La prueba dirigida `nsc.driver-address-sanitizer-throwing-array-release` añade
evidencia de liberación física para los dos arreglos dinámicos anteriores. En
cada uno guarda una dirección dentro del bloque, la lee mientras está vivo,
deja que falle un destructor y comprueba la traza de limpieza antes de leerla
deliberadamente otra vez. ASan detectó `heap-use-after-free` y terminó con 1 en
O0/O2 para elementos struct y de clase; la prueba registrada pasó 1/1
(`asan-throwing-array-registered.log`). Se usó el compilador SHA-256
`392aa7ff1e8a85459f776351bd798a39e33b646501f31cee1428442df4a1fd7d`,
el adaptador de heap ASan, Clang y runtime de referencia, y `detect_leaks=0`.
La lectura inválida existe solo en este fixture instrumentado. Esto acredita la
liberación del bloque de esos arreglos en esa configuración, no la ausencia de
otras fugas, la liberación de cada payload ni los recorridos no probados.

## Prototipos de las familias

Después de corregir la pérdida de imports en métodos genéricos se probaron, en
la carpeta de evidencia, prototipos de `Rc`/`WeakRc` y `Arc`/`WeakArc` que
envuelven los controles internos. Para cada par pasaron en O0/O2 los casos de
clonación, contadores, degradación, upgrade, expiración, destrucción única,
payload de estructura y vista; los casos probados de escape y movimiento del
propietario se rechazaron. Los resultados están en
`A:/nsc-fixes-evidencia/public-rc-wrapper-probe/results-all.json` y
`A:/nsc-fixes-evidencia/public-arc-wrapper-probe/results-all.json`.

Son prototipos locales de la superficie, no implementaciones públicas del
SDK. En particular, no prueban envío entre hilos con el tipo real ni retención
del módulo productor durante toda la vida del bloque.

## Superficie pública inicial en el árbol de trabajo

Los tipos `Rc`, `WeakRc`, `Arc`, `WeakArc`, `SharedView` y las fábricas de
`SharedPointers` están implementados inicialmente en fuentes de Core. La
prueba dirigida `nsc.sdk.shared-public-pointers` pasó con uso, vistas,
clonación, degradación y upgrade, expiración, clases, estructuras y fábricas
que propagan excepciones en O0/O2 (`shared-public-pointers-ctest.log`). La
reproducción original del reporte, con sus tres archivos `.ns` intactos y solo
la referencia del proyecto copiado adaptada a Core en A, pasó el análisis de
cuerpos (`original-smart-surface/result.json`). Son pruebas desde fuentes.

La misma reproducción original, sin modificar sus tres archivos `.ns`, se
emitió y ejecutó posteriormente en O0 y O2 con el compilador aislado y la
referencia de proyecto a Core en A; las cuatro operaciones terminaron con
código 0 (`A:/nsc-fixes-evidencia/original-smart-surface/emit-results.json`).
El programa solo retorna cero y declara campos de los cuatro tipos: no prueba
operaciones de los punteros, concurrencia ni consumo del SDK binario.

Una reproducción pública desde fuentes conserva un `WeakRc` en un campo
estático y consulta su conteo desde el destructor del último `Rc`: observó uno,
quedó expirado y mantuvo conteo uno después de la destrucción. Se ejecutó con
salida 42 en O0 y O2 (`public-rc-reentrant-count/results.log`); la selección
correspondiente de `shared_public_pointers.py` pasó también
(`public-rc-reentrant-count-ctest.log`). Esto verifica la ruta pública desde
fuentes después de `49bf24d2a`, no consumo binario ni el contrato de `Arc`.
Con una copia del control anterior a ese commit, la misma reproducción devolvió
1 en O0 y O2 (`public-rc-reentrant-count/baseline.log`).
La prueba pública de alias de solo lectura dentro de una tupla detectó una
pérdida de la marca en `Rc` y `Arc`; tras la corrección ambas formas se rechazan.
El alcance conservador y los controles están en `readonly-array-borrow.md`.
Las formas con llaves de la tupla y de un struct sin ownership también
descartaban la marca; la corrección posterior las rechaza en ambas familias.

En el árbol de trabajo se añadieron tres regresiones semánticas para cada
familia fuerte: rechazo del retorno de una vista prestada de un propietario
local, del retorno prestado de una clase extraída de la vista y de la escritura
por ese alias. Las seis pasaron usando el selector dirigido de
`shared_public_pointers.py` (`A:/nsc-fixes-evidencia/public-view-regressions.log`).
El análisis separado con `ProjectReference` a Core comprobó, además, las
formas de retorno de clase directa y sin calificador
(`public-view-escape-probe/results.json`). No son pruebas de consumo binario.
La prueba dirigida de uso se repitió con la caché de artefactos habilitada:
`Rc` y `Arc` terminaron con 42 en O0 y O2
(`public-pointers-safe-incremental.log`).
Para propietarios vacíos de ambas familias, `Get()` y `Borrow().Get()`
terminaron con el trap esperado 142 en O0 y O2: ocho ejecuciones dirigidas
registradas en `public-empty-regressions.log`. Es la política de abortar
utilizada por esa configuración; no acredita otros perfiles de panic.
`WeakRc` y `WeakArc` inicializados por defecto se observaron expirados, con
conteo débil cero y upgrade vacío; sus cuatro ejecuciones O0/O2 terminaron en
42. La copia implícita de cada uno fue rechazada durante el análisis semántico
(`public-weak-default-regressions.log`).
Otra regresión dirigida mueve una referencia débil, destruye el último fuerte,
clona y vuelve a mover la débil vencida, y comprueba que `Upgrade` devuelve
un propietario vacío, que los conteos débiles pasan de uno a dos y vuelven a
uno, y que el payload se destruyó una sola vez. Pasó para `WeakRc` y `WeakArc`
en O0 y O2 (`public-expired-weak-clone-move.log`). La prueba es secuencial y
no demuestra la liberación del bloque ni el comportamiento concurrente.
Se comprobó también que las fábricas ordinarias rechazan constructores que
declaran excepciones, tanto para clases como para estructuras: las cuatro
regresiones dirigidas de `shared_public_pointers.py` exigen el diagnóstico
`constructor throws` durante la emisión O0
(`nonthrowing-factory-regressions.log` y
`nonthrowing-value-factory-regressions.log`). La comprobación semántica
aislada puede aceptar inicialmente el caso: el rechazo aparece al
especializar y emitir el método. Una reproducción con `ProjectReference` a
Core registró ese desfase y procesó sus 72 archivos
(`nonthrowing-factory-probe/results.json`); las regresiones posteriores usan
el conjunto mínimo de fuentes y conservan la caché habilitada.
En esa misma reproducción, copiar un campo de clase anidado a un local sin
calificador se rechazó porque perdería el préstamo. La variante que declara
el local `borrowed` también se rechazó, pero por no aceptar esa expresión
como origen directo de vida; esto no demuestra una comprobación de escritura
en el alias anidado.
Para `Rc` y `Arc`, la captura por copia o por referencia de un alias prestado
en una lambda que intenta escribir fue rechazada; en los cuatro casos el
diagnóstico también indicó que el campo no es asignable. Las fuentes y ambos
diagnósticos se conservan en `shared-readonly-capture-probe/results.json`; las
cuatro regresiones dirigidas están en `public-readonly-capture-regressions.log`.
Estos casos no prueban todas las formas de captura ni consumo binario.
Los controles dirigidos `async_mutate_after_await` y `async_move_after_await`
rechazaron respectivamente la escritura mediante el alias y el movimiento del
propietario con una vista aún utilizada después de `await`, tanto para `Rc`
como para `Arc`. El control `async_read_after_await` conservó una vista y leyó
su carga después de una suspensión, con salida 42 en O0 y O2 para ambas
familias. Se ejecutaron mediante
`A:/nsc-fixes-evidencia/shared-async-borrow-check.cmd` usando Core desde
fuentes; no prueban consumo binario, transferencia entre hilos ni todas las
formas de préstamo en corrutinas.
Una reproducción adicional acepta reemplazar un parámetro `Rc` o `Arc` pasado
por valor mientras una vista creada desde ese parámetro sigue viva. Otra acepta
mover el parámetro `Rc` en la misma situación. Las comprobaciones fueron solo
semánticas; no se ejecutaron los accesos potencialmente inválidos. Las dos
regresiones pendientes están en `shared_public_pointers.py`. El seguimiento de
raíces de vistas de `viewRootLocalOfExpression` reconoce receptores locales,
pero no registra como raíz el parámetro de estas reproducciones;
`hasActiveBorrowOfOwnedParameter` consulta otras tablas de préstamos y no la
raíz de esas vistas. La causa y una corrección aún requieren verificar cómo
conservar la identidad del parámetro, la sombra de nombres y las vistas
reencadenadas sin alterar los préstamos válidos.
Después de esa observación, `7f69e7643` añade el seguimiento para parámetros
move-only por valor y protege su reemplazo y movimiento. Las pruebas y los
límites restantes están en `move-only-parameter-view.md`.
La inspección de una referencia local a una carga struct probó dos sintaxis:
`ref readonly Value` se rechaza al parsear, y `ref const Value` se rechaza como
referencia local mutable que no puede ser const y que requiere almacenamiento
mutable. Esto no demuestra que no exista otra forma admitida ni afecta la
lectura directa `owner.Get().Data`, que ya se ejecutó. No se ejecutó un acceso
inválido ni se modificó el contrato del lenguaje; queda por identificar una
forma admitida de conservar la referencia de solo lectura antes de probar su
vida como local.
En el código actual, `classifyVariableReferenceLocal` marca como solo lectura los
locales sintetizados para `foreach ref` y enlaces de patrón `ref`; la declaración
ordinaria `local:ref` se clasifica como referencia mutable. El parser ordinario
reconoce `ref` antes del tipo y no registra un modificador `readonly` en esa
declaración.

La prueba dirigida `struct_pattern_ref_read` ensaya
`match (owner.Get()) { ref item => ... }` para una carga `Value` en cada familia
fuerte. Tanto `Rc` como `Arc` se rechazan durante el análisis con
`reference-local initializer must be a mutable addressable storage location`;
no se ejecutó ese caso. El control `struct_pattern_ref_local_control`, sobre
un `Value` local mutable, pasó en O0 y O2 en ambas iteraciones de la prueba.
Las ejecuciones utilizaron `A:/nsc-fixes-evidencia/shared-struct-pattern-check.cmd`
y el compilador aislado; el selector se configuró por caso y familia. La salida
se observó directamente, sin log independiente conservado.

En las dos implementaciones del parser, el lowering de `match` sintetiza un
`local:ref` mutable para la expresión de entrada siempre que encuentra un
enlace `ref` o `ref mut`; el enlace posterior sí distingue ambos modos. Esta
observación explica el rechazo del receptor de solo lectura, pero no prueba que
baste cambiar el local sintético: habría que conservar los patrones mutables,
los enlaces mezclados y las restricciones de vida antes de corregirlo. La
regresión sigue esperando éxito y permanece sin resolver. Esta evidencia no
determina si `foreach` puede enlazar el resultado concreto de `Get()` ni define
una nueva sintaxis para una referencia local general.

Se ensayó marcar el sujeto sintético como `local:pattern-ref` cuando ningún
brazo exige `ref mut`. El compilador incremental aceptó y ejecutó la lectura
de Rc en O0/O2 y rechazó la escritura directa, pero permitió reemplazar el
parámetro propietario dentro del brazo mientras `item` seguía vivo. La misma
aceptación incorrecta se observó en Arc; solo se ejecutó el análisis, no el
programa peligroso. El ensayo se retiró de ambas rutas del parser y el
compilador aislado se reconstruyó incrementalmente. La regresión
`struct_pattern_ref_overwrite` conserva el resultado esperado de rechazo.
Tras la reversión pasaron los controles de patrones `ref` y `ref mut` sobre
un struct local en O0/O2, y el análisis rechazó `ref mut` sobre `owner.Get()`
para Rc y Arc. Son controles dirigidos, no una certificación de la corrección.
El análisis de inicialización de variables actualmente da tratamiento especial
a `local:foreach-ref` para registrar ciertos préstamos de solo lectura; todavía
no está demostrado que aplicar ese tratamiento a patrones cubra sus fuentes,
rebindings y ámbitos. No se presenta el ensayo retirado como corrección.

Un segundo ensayo conserva el propietario de una llamada de instancia `ref`
con vida `this` al crear el sujeto sintético de `match`, incluso cuando el
struct no contiene referencias y no tiene la marca `readonlyBorrowedObject`.
En las pruebas dirigidas desde fuentes, para Rc y Arc, la lectura directa y la
lectura a través de una vista ejecutaron 42 en O0/O2. El análisis rechazó la
escritura, el reemplazo o movimiento del propietario mientras el patrón estaba
activo, el origen temporal y la extracción o escritura de una clase interna.
El reemplazo después del `match` y los patrones `ref mut` sobre un local
mutable pasaron en O0/O2. La extracción como `borrowed Child` fue rechazada por
la limitación preexistente de fuentes de préstamos de campos; no demuestra que
esa forma válida esté admitida. Los controles existentes de patrones
fundamentales, exhaustividad y guardas también pasaron tras compilar sus targets
incrementalmente. La salida se observó directamente; no se guardó log aparte.
Este ensayo permanece sin commit: aún deben comprobarse receptores distintos,
patrones mixtos y escapes, y separarlo de los otros cambios en curso.

El control `pattern_ordinary_receiver_control` detectó que el ensayo también
rechazaba `match (box.Get())` para un objeto ordinario cuyo método devuelve
`ref[this] int`. Se limitó la nueva ruta de registro a receptores move-only o
vistas. Tras la compilación incremental, ese control y
`pattern_mixed_mutable_control` ejecutaron 42 en O0/O2; el conjunto dirigido de
16 casos se volvió a ejecutar para Rc y Arc y pasó. La salida se observó
directamente, sin log independiente. Los controles ordinarios se repiten por
familia debido al runner, aunque no usan punteros compartidos. No se ha
demostrado el comportamiento para todos los receptores ni todas las formas de
escape; los cambios permanecen provisionales y sin commit.
Dos controles dirigidos adicionales rechazaron, para Rc y Arc, mover un dueño
local durante el patrón y devolver la clase extraída del struct. La prueba
comprueba el rechazo semántico y no ejecuta los programas rechazados.
Para una carga de clase, los controles de lectura directa y mediante vista
ejecutaron 42 en O0/O2 con Rc y Arc; el análisis rechazó escribir, devolver el
objeto prestado y reemplazar al propietario durante el patrón. También rechazó
reasignar una vista, reemplazar su propietario anterior y usar la referencia
que seguía activa. La prueba negativa solo exige el diagnóstico de reemplazo;
no acredita por sí sola todos los posibles rebindings de vistas.
El análisis rechazó también una llamada a método potencialmente mutante sobre
la clase prestada y un retorno `ref` al campo de un struct prestado, en ambas
familias. El segundo caso produjo `ref return cannot expose a by-value or
readonly parameter`; el texto completo se comprobó en la salida de la prueba.
La corrección del patrón y una regresión independiente con un propietario
move-only mínimo quedaron en `900ef6312`. La prueba dirigida y los controles
pasaron en el árbol con otros cambios pendientes; no se hizo un build aislado
de ese commit, por lo que la evidencia tiene esa limitación.

Al ampliar las reproducciones de `match`, tanto Rc como Arc rechazan en análisis
`match (holder.value.Get()) { ref item => ... }` cuando el propietario está en
un campo, y también `match (owner.Borrow().Get()) { ref item => ... }` con una
vista temporal. Ambos diagnósticos son `borrowed local requires a direct live
owner or borrowed identifier source`; no se ejecutaron. Las pruebas
`struct_pattern_field_read` y `struct_pattern_chained_view_read` conservan la
lectura pretendida. Para el campo, admitirla sin rastrear el reemplazo directo,
alias y llamadas que alteran el campo dejaría una referencia colgante; la regla
para llamadas sigue pendiente de decisión. En la cadena, la referencia está
ligada por contrato a la vista temporal; se consultó si debe exigirse una vista
con nombre o extender la vida del temporal hasta terminar el match. Hasta la
decisión no se clasifica el rechazo de esa cadena como defecto de contrato.
Las salidas se observaron directamente, sin log independiente.
Actualización: la lectura encadenada con dueño local o parámetro move-only y
vista temporal sin drop pasó para Rc y Arc, junto con controles de reemplazo y
movimiento (`readonly-chain-families.log`). La decisión, la implementación y
sus límites se registran en `readonly-pattern-loan.md`; el caso del dueño
guardado en un campo sigue abierto y detiene la prueba completa de punteros
(`readonly-chain-pointer-control.log`).
Actualización posterior: la lectura de un dueño en campo directo de un local
RAII y los controles de alias, movimiento, llamadas y fin del patrón pasaron
por separado para Rc y Arc (`readonly-field-pattern-families.log`). El alcance
y los casos no cubiertos constan en `readonly-pattern-loan.md`; no se volvió a
ejecutar la prueba completa de punteros tras ese cambio.
Los controles públicos desde fuentes de una carga struct con un campo de clase
comprueban el propietario recibido por valor tanto con `Get()` como mediante
`Borrow().Get()`. Para `Rc` y `Arc`, el análisis rechazó copiar o modificar la
clase interna y la lectura del campo escalar ejecutó con salida 42 en O0/O2.
La selección está en `shared_public_pointers.py`; estos casos no demuestran el
almacenamiento de una referencia local al struct ni consumo binario.

Actualización: el conjunto de análisis de campos y movimientos descrito a
continuación quedó registrado en `95aac1e18`, con la regresión independiente y
sus límites en `field-view-storage.md`. Las anotaciones históricas que lo
llaman ensayo sin commit reflejan el estado al momento de cada prueba; el
resto de las familias públicas y las decisiones pendientes siguen abiertos.

Se reprodujo otra pérdida de procedencia cuando un método de instancia devuelve
una vista de uno de sus campos: `holder.View()` permitía sustituir directamente
`holder.value` con la vista viva. También sucedía con
`holder.child.View()` y la sustitución de `holder.child.value`. Los ejemplos
inseguros se analizaron sin ejecutarlos. El ensayo ahora registra el receptor
nominal como raíz y, cuando no es un propietario move-only cuyo campo ya se
conoce, considera conservadoramente sus campos como posibles fuentes. Para
receptores anidados conserva la raíz exterior y la misma incertidumbre.

Tras compilar incrementalmente, el control dirigido rechazó las sustituciones
directas, a través de un alias y de dos parámetros prestados que podrían referir
al mismo objeto, tanto para Rc como Arc. También rechazó el acceso anidado y
una sustitución de otro campo del mismo receptor, al no demostrar qué campo
devuelve el método. Los controles de otro objeto y de sustitución después de
terminar la vista ejecutaron 42 en O0/O2; pasaron además controles anteriores
de campos, vistas encadenadas y condicionales. Evidencia:
`A:/nsc-fixes-evidencia/nested-method-view-after.log`. Las tres pruebas
existentes de préstamos locales, escape y vida de vistas pasaron tras
recompilarse incrementalmente (`method-view-existing-tests.log`).

También se comprobaron receptores de método obtenidos de un arreglo local y
de un arreglo que es campo de otro objeto. El análisis rechazó sustituir el
campo prestado en ambas formas para Rc y Arc; el control de sustitución después
de terminar la vista ejecutó 42 en O0/O2. Para una vista devuelta por un método,
el análisis también rechazó escribir en la clase extraída tanto directamente
como mediante un alias, y rechazó devolver esa clase desde el ámbito del dueño.
Estas son comprobaciones de rechazo semántico; no se ejecutaron los accesos
inválidos. Registro: `A:/nsc-fixes-evidencia/method-view-readonly-controls.log`.
El tratamiento de arreglos es conservador y no acredita índices independientes.

Una reproducción adicional aceptaba sustituir un campo después de obtener
una vista mediante `this.value.Borrow()`; el mismo problema se observó con
`value.Borrow()` y con `View()` implícito que devuelve una vista del receptor.
El ensayo identifica ahora el receptor actual en los accesos explícitos e
implícitos, y considera que un parámetro prestado puede ser ese mismo objeto.
Las reproducciones semánticas de Rc y Arc se rechazan para las dos direcciones
de ese posible alias y para las combinaciones de acceso explícito e implícito.
Un intento de crear `borrowed Holder alias = this` fue rechazado antes por la
regla de origen del préstamo; no demuestra seguimiento de ese alias. Los
controles de otro campo y de sustitución tras terminar la vista ejecutaron 42
en O0/O2 (`A:/nsc-fixes-evidencia/this-view-final-controls.log`). También
pasaron los controles de campos y métodos previos (`this-view-previous-controls.log`)
y las tres pruebas existentes de préstamos (`this-view-existing-tests.log`).
Esto no acredita identidades de objetos obtenidos por llamadas arbitrarias ni
resuelve llamadas potencialmente mutantes mientras existe el préstamo. La
aproximación conserva su carácter provisional y no se ha registrado en commit.

También se reprodujo que `child.value.Borrow()` dentro del propio objeto
permitía sustituir `child.value`: el nombre `child` era un campo implícito y
no se identificaba como raíz del receptor. El ensayo reconoce esa raíz usando
su enlace semántico y conserva el campo intermedio. Para Rc y Arc, el análisis
rechazó la sustitución directa, las dos combinaciones de `child` y
`this.child`, y la sustitución del campo intermedio `owned`; el control tras
terminar la vista ejecutó 42 en O0/O2 (`implicit-nested-view-expanded.log`).
Pasaron los controles previos de `this` y de receptores locales y las tres
pruebas existentes de vida y escape (`implicit-nested-this-controls.log`,
`implicit-nested-previous-controls.log` e `implicit-nested-existing-tests.log`,
en `A:/nsc-fixes-evidencia`). No se ejecutaron los casos inseguros. Esta
evidencia se limita a las rutas y alias probados; el ensayo sigue sin commit.

Una copia de referencia de clase, `Inner alias = child`, también permitía
sustituir por `alias.value` el propietario sobre el que existía la vista. El
ensayo reconoce como posible alias del receptor los locales de clase
inicializados o reasignados directamente desde uno de sus campos, y sigue
iterativamente cadenas de inicializadores locales. Tras compilar
incrementalmente, los controles dirigidos rechazaron el alias inicializado
desde `child` y `this.child`, una reasignación desde `child` y una cadena de
dos alias, para Rc y Arc. El control con un objeto independiente ejecutó 42
en O0/O2 (`nested-class-alias-final.log`). Se repitieron los controles previos
de vistas y las tres pruebas existentes de préstamos
(`nested-alias-previous-controls.log`, `nested-alias-existing-tests.log`).

La marca de reasignación es conservadora y permanece incluso si el alias
cambia después a otro objeto. No se han cubierto todas las reasignaciones que
copian otros locales, resultados de llamadas ni alias guardados en agregados;
este ensayo no demuestra seguridad general de alias de clase. Los casos
inseguros solo se analizaron y el conjunto continúa sin commit.

La reasignación `second = first` perdía el origen cuando `first` era alias de
un campo; también se reprodujo con una expresión condicional. El ensayo
conserva ahora las posibles fuentes locales de una referencia y recorre las
ramas de inicializadores y asignaciones condicionales mediante una lista
iterativa; evita ciclos en la consulta. Para Rc y Arc, el análisis rechazó
las reproducciones de asignación, condicional, ramas y ciclo; un objeto
independiente ejecutó 42 en O0/O2 (`conditional-class-alias-final.log`).
También pasaron los controles previos y las tres pruebas existentes
(`conditional-alias-previous.log`, `conditional-alias-existing.log`).

El seguimiento une fuentes sin distinguir momentos o caminos y puede
rechazar reasignaciones seguras. No acredita resultados de llamadas ni todas
las clases de expresión, almacenamiento o transmisión entre funciones. Al
añadir la información al encabezado compartido, la compilación incremental
recompiló 206 pasos, incluidos objetos de metadata e interoperabilidad en el
build aislado de A; no se cambiaron sus fuentes ni las carpetas de build de
interoperabilidad. La implementación permanece provisional y sin commit.

Otro caso aceptado obtenía una referencia de un campo de un objeto local,
`Inner alias = holder.child`, y después sustituía `alias.value` mientras una
vista de `holder.child.value` seguía viva. El ensayo recorre el receptor de
campos explícitos y registra sus posibles raíces locales o parámetros,
incluidas las reasignaciones y ramas condicionales comprobadas. Para Rc y Arc,
las reproducciones rechazaron las sustituciones; los controles de otro objeto
y de sustitución después de la vista ejecutaron 42 en O0/O2. Evidencia en
`object-field-class-alias-final.log` y `parameter-field-alias-after.log` bajo
`A:/nsc-fixes-evidencia`. Pasaron además los controles previos de condicionales
y tres pruebas existentes (`object-field-conditional-controls.log` y
`object-field-existing-tests.log`). Las reproducciones inseguras solo se
analizaron. Dos expresiones que parten del mismo objeto pueden considerarse
alias aun cuando sus campos sean independientes; no se demuestra precisión
de rutas ni seguridad para fuentes arbitrarias. El ensayo sigue sin commit.

La incorporación de la fuente de parámetros en el encabezado compartido
provocó otros 206 pasos incrementales en el build aislado, incluidos objetos
de metadata e interoperabilidad; no se cambiaron esas fuentes ni sus carpetas
de build. Se mantiene la política de pruebas dirigidas.

Una referencia copiada desde un elemento de arreglo, como
`Holder alias = holders[0]`, permitía sustituir un campo que también estaba
prestado mediante `holders[0]`. También se aceptaba cuando primero se copiaba
la referencia al arreglo. El ensayo recorre la fuente indexada y las cadenas
de arreglos dinámicos, incluyendo reasignaciones. Para Rc y Arc se rechazaron
las reproducciones y pasaron los controles con arreglo local, campo, parámetro,
reasignación, dos arreglos independientes y sustitución después del préstamo;
los controles seguros ejecutaron 42 en O0/O2 (`array-class-alias-all.log`).
Pasaron también los controles anteriores de campos y las tres pruebas
existentes (`array-class-alias-prior.log`, `array-class-alias-existing.log`).
Los casos inseguros se analizaron sin ejecutarlos. La fuente se aproxima por
arreglo completo y no distingue índices independientes; tampoco acredita todas
las formas de receptores calculados. El seguimiento continúa provisional y sin
commit.

Los controles adicionales `array_element_class_alias_reverse` y
`array_element_class_alias_conditional` también pasaron para Rc y Arc: el
análisis rechazó sustituir el campo desde el arreglo cuando la vista se obtuvo
por el alias, y desde un alias elegido condicionalmente. El control se ejecutó
con las demás variantes de arreglo (`array-class-reverse-conditional.log`).

Se investigó además el movimiento del propietario prestado. Mover directamente
`holder.value` mientras vive una vista del campo se rechaza; al terminar la
vista, el movimiento ejecuta 42 en O0/O2 para Rc y Arc
(`move-borrowed-field-after.log`). Sin embargo, las reproducciones
`move_borrowed_owner_field_alias`, `move_borrowed_this_field` y
`move_borrowed_array_element` fueron aceptadas por el análisis de Rc, aunque
pueden hacer que la vista sobreviva al último propietario. La variante de
alias también se aceptó al comprobarla por separado con Arc
(`move-borrowed-alias-arc.log`); las otras dos aún no se comprobaron en Arc.
No se ejecutaron los programas
inseguros. Evidencia en `move-borrowed-field-alias-before.log`,
`move-this-field-before.log` y `move-array-borrowed-before.log`.
`rejectInvalidPartialMove` consulta raíces de locales y parámetros mientras el
control provisional de sustitución de campos usa una comparación de raíces y
campos más amplia en `prepareAssignmentTarget`. Las rutas de movimiento de
campos implícitos también pueden llegar sin raíz local. La corrección debe
compartir o aplicar de forma equivalente esa comprobación, sin impedir el
movimiento de campos independientes ni después de terminado el préstamo; no
se ha implementado todavía y estas regresiones siguen pendientes.

Actualización posterior: el ensayo extrajo la comparación de raíces y campos
de sustitución a una consulta común y la aplica al movimiento de campos y
elementos, incluidos campos implícitos. Para enlazar correctamente la raíz
implícita de un miembro anidado, el análisis de ese miembro ocurre antes de
consultar el préstamo. La compilación incremental terminó correctamente.
En Rc y Arc se rechazaron las seis reproducciones dirigidas: campo directo,
alias, `this`, campo implícito, campo implícito anidado y elemento de arreglo.
Los controles de movimiento después del préstamo, otro objeto y otro campo
accedido mediante alias ejecutaron 42 en O0/O2
(`A:/nsc-fixes-evidencia/move-borrowed-field-final.log`). Pasaron las pruebas
dirigidas anteriores de sustitución, las tres pruebas existentes de préstamos
y `nsc.ns3.t06.partial-moves` tras compilar sus objetivos
(`move-final-field-controls.log`, `move-final-existing-tests.log` y
`move-common-partial-existing.log`).

La consulta comparte los límites del seguimiento provisional: es
conservadora para arreglos y algunas rutas de alias y no demuestra todas las
fuentes calculadas o accesos entre funciones. No se ha demostrado la precisión
de mover directamente otro campo desde la misma raíz. El código permanece
sin commit con el resto del ensayo; no se ha validado de forma independiente
del árbol de trabajo en curso. La primera compilación incremental recompiló 206
pasos por el encabezado compartido, incluidos objetos de interoperabilidad en
el build aislado; no se cambiaron sus fuentes ni builds separados.

Una reproducción posterior mostró que `move(items[0])` seguía aceptándose
cuando `items` era un campo implícito y una vista del elemento estaba viva.
La consulta se hacía antes de enlazar el nombre y la fuente de la vista no
reconocía ese arreglo como campo de `this`. El ensayo enlaza el elemento antes
de consultar y conserva la raíz del receptor también para arreglos implícitos.
En Rc y Arc se rechazaron las variantes implícita y explícita; el movimiento
después del préstamo ejecutó 42 en O0/O2. Pasaron además las reproducciones
y controles anteriores de movimientos (`move-array-field-final.log`), la
prueba existente de movimientos parciales (`move-array-partial-existing.log`),
los controles de sustitución (`move-array-field-prior.log`) y las tres pruebas
de vida (`move-array-lifetime-existing.log`), todos en
`A:/nsc-fixes-evidencia`. El control de la reproducción insegura fue solo
semántico. El seguimiento conserva los límites y el estado sin commit descritos
arriba; considera el arreglo completo y no acredita todos los receptores.

Esta aproximación puede rechazar modificaciones seguras de otros campos del
mismo receptor y no demuestra procedencia de todos los receptores calculados.
Tampoco decide ni corrige las llamadas a métodos que podrían modificar campos
mientras vive una vista: esa pregunta continúa pendiente. El cambio depende
del seguimiento provisional de campos en el mismo archivo y permanece sin
commit junto con él; las pruebas usaron el árbol completo en curso.

Una reproducción de seguridad posterior acepta reemplazar `holder.value`
mientras sigue viva una vista derivada de ese campo, tanto para `Rc` como
para `Arc`. También se acepta el reemplazo mediante un alias `borrowed Holder`
del mismo objeto para `Rc` y `Arc`. Solo se ejecutó el análisis semántico; no se ejecutó
el acceso posterior a la vista ni se demostró si el reemplazo libera el valor
anterior. Los controles que reemplazan otro campo del mismo objeto o esperan
al fin del ámbito de la vista ejecutaron con 42 en O0/O2 para ambas familias.
Las regresiones en preparación están en `shared_public_pointers.py`. El
seguimiento actual de la vista registra la raíz local, pero no la identidad
completa del campo; comparar solo el nombre de la raíz dejaría fuera el alias y
rechazaría el control con un campo independiente. La corrección debe resolver
ambos casos antes de considerarse segura.
`storageIdentifierPlaceKey` asigna una clave distinta a cada local de clase,
incluido un local `borrowed` que apunta al mismo objeto; por su parte,
`configureBorrowedVariableSource` sí conserva el ordinal del dueño y los dueños
posibles del alias. La representación de vista registra actualmente solo el
nombre de la raíz. Para la corrección queda por enlazar esa información con la
identidad del campo y contemplar reasignaciones del alias y de la vista; no se
ha aplicado un rechazo que solo compare nombres de locales.
También se reprodujo que una vista puede reasignarse del primer campo al segundo
del mismo objeto y que luego se acepta reemplazar el segundo mientras la vista
sigue viva, en Rc y Arc. La prueba es solo semántica. Como
`recordViewAssignmentRoot` compara la raíz del objeto, ambos campos comparten
la misma raíz y la representación no conserva cuál fue la última fuente ni
las fuentes posibles tras una unión de flujo. La corrección debe registrar o
aproximar conservadoramente los campos posibles sin perder los controles que
reemplazan otro campo independiente.
Un ensayo sin commit conserva nombres de campos y raíces posibles de vistas y
consulta los orígenes rastreados de aliases prestados. Las pruebas desde fuentes
rechazaron las cuatro reproducciones de reemplazo directo, mediante alias,
vista reasignada y alias reasignado para Rc/Arc; los dos controles de campos
independientes y fin de ámbito ejecutaron con 42 en O0/O2. Un control adicional
de dos parámetros prestados que podrían apuntar al mismo objeto también se
rechazó en ambas familias. El ensayo sigue abierto: hay que verificar raíces
desconocidas, campos superpuestos, cadenas de vista y otros accesos antes de
considerarlo seguro. La compilación incremental del compilador recompiló 206
pasos por un encabezado compartido; incluyó objetos de metadata bajo el build
aislado de A. No se modificaron fuentes ni carpetas de build de interoperabilidad.
La reproducción de un campo anidado `holder.child.value` también era aceptada;
el ensayo se amplió para alcanzar la raíz a través de miembros anidados y la
regresión pasó para Rc/Arc. Se repitieron los controles directos, de alias,
reasignación y campos independientes con éxito. Sin embargo, `holder.Replace()`
puede reemplazar el campo desde otro método y el análisis aún acepta la llamada
mientras existe la vista (reproducción semántica para Rc, sin ejecución). Se
consultó el contrato de llamadas durante el préstamo; la decisión está pendiente
y el ensayo no está completo ni se ha registrado en un commit.
Una vista copiada a otro local conservó el bloqueo de reemplazo en los dos
controles semánticos. Otra reproducción con `holder.values[0].Borrow()` aceptaba
el reemplazo del mismo elemento; el ensayo ahora la rechaza para Rc/Arc. Se
trata el campo arreglo como unidad conservadora, por lo que otros índices del
mismo campo pueden rechazarse; no se ha acreditado precisión de índices ni
accesos sin raíz nominal. Los controles de campos anteriores volvieron a pasar
después del cambio. La llamada indirecta `holder.Replace()` sigue pendiente de
la decisión consultada y no se presenta como corregida.
Se reprodujo el mismo reemplazo para un arreglo local nominal. El ensayo lo
rechaza para Rc/Arc y permite reemplazar el elemento una vez terminado el
ámbito de la vista, con ejecución 42 en O0/O2. Los controles dirigidos de
campos, alias y arreglos de campo también pasaron tras el ajuste. Se considera
todo el arreglo local como una unidad conservadora: no se distingue un índice
independiente y no se acredita el caso de una fuente calculada o desconocida.
También se reprodujo la sustitución de un campo `owned` intermedio de la ruta
de la vista. El ensayo registra los miembros intermedios y rechazó ese caso
para Rc/Arc; permitió sustituir otro campo y sustituir el campo intermedio tras
terminar la vista, con salida 42 en O0/O2. La comparación por nombres de
miembro es conservadora y puede rechazar rutas distintas con nombres repetidos.
La comprobación de métodos y fuentes no nominales continúa abierta.
Una unión con dos campos compartidos superpuestos permitía analizar una vista
del primer campo y el reemplazo del segundo. El ensayo trata conservadoramente
los miembros de la unión como solapados y rechaza esa reproducción para Rc y
Arc; no se ejecutó. Un control sin vista pasa análisis pero falla al emitir en
O0 para ambas familias: el backend exige un discriminador de miembro activo
para destruir la unión (`union-without-view-build-{Rc,Arc}.log`). La comparación
con el compilador base no llegó a esa etapa porque rechazó antes las llamadas
genéricas de las fuentes actuales (`union-without-view-baseline-{Rc,Arc}.log`);
no se atribuye ese rechazo de emisión al ensayo ni se considera el control
aprobado. Los offsets explícitos quedan sin reproducción propia.
Una reproducción independiente con `union Holder { Guard first; Guard second; }`,
donde `Guard` tiene destructor y no utiliza las familias compartidas, falla al
emitir en O0 tanto en la revisión base como en la rama con el mismo diagnóstico
de discriminador de miembro activo (`union-value-drop-baseline/results.json` y
logs asociados). Esto confirma que esa limitación existe en la base para la
forma probada; no demuestra equivalencia para todos los tipos o configuraciones.
Se preparó después un contraste de offsets explícitos: con ambos campos en
`at(0)`, el ensayo rechaza el reemplazo con el diagnóstico de vista viva para
Rc y Arc. Con campos en `at(0)` y `at(64)`, también lo rechaza; el control sin
vista ejecuta 42 en O0/O2 para ambas familias. Por tanto el ensayo es más
restrictivo incluso para esa separación aparente. La comprobación actual
considera solapado cualquier tipo con offsets explícitos y no calcula rangos;
el análisis de cuerpos no expone directamente el modelo de layout consultado.
Se consultó el contrato para permitir rangos cuya separación pueda demostrarse;
no se afirma todavía que el caso con vista complete la emisión o ejecución.
Después de compilar incrementalmente sus tres ejecutables de prueba,
`view-existing-directed.cmd` pasó las pruebas existentes de vida de préstamos
locales, escape de préstamos y vida de vistas (`nsc.stage50_5`, `nsc.stage50_7`
y `nsc.stage58`). Estas verificaciones del análisis no resuelven la llamada
indirecta pendiente ni acreditan el resto de formas de procedencia.
La selección `field_view_overwrite`, `field_view_alias_overwrite`,
`field_view_reassignment` y `field_view_other_field` de la prueba registrada
`nsc.compiler.readonly-borrowed-array` pasó con los controles internos local y
atómico: rechazó las tres formas inseguras y ejecutó el campo independiente con
salida 42 en O0/O2. Es evidencia adicional del ensayo, que permanece sin commit.
Los controles `overwrite_field_view_root` y `move_field_view_root` comprueban
la raíz de un objeto con una vista viva sobre uno de sus campos: el análisis
rechazó reemplazarla o moverla para Rc y Arc. Las variantes después del ámbito
de la vista ejecutaron 42 en O0/O2 para ambas familias. La evidencia está en
`A:/nsc-fixes-evidencia/shared-view-root-lifetime.log`. El primer ensayo de
movimiento usó un destino sin ownership y fue rechazado por una regla distinta;
los controles finales usan un destino `owned` y una asignación mediante `move`.
Esto no resuelve llamadas a métodos que sustituyan el campo ni alias arbitrarios.

Una vista inicializada con `choose ? holder.first.Borrow() : holder.second.Borrow()`
permitía sustituir cualquiera de los campos mientras la vista seguía viva; la
reproducción se limitó al análisis semántico y no ejecutó el acceso inválido.
`collectViewSourceFields` registraba la expresión condicional como origen
desconocido sin conservar las raíces de sus ramas. El ensayo ahora recorre
iterativamente las ramas de valor y acumula sus campos y raíces, tanto en la
declaración como en una reasignación de vista. La compilación incremental de
`nsc` terminó correctamente; `shared-struct-pattern-check.cmd` rechazó la
sustitución de cada rama y la reasignación, para Rc y Arc. También ejecutó con
salida 42 en O0/O2 los controles de un tercer campo y de sustitución después de
terminar el ámbito, además de los controles previos de campos anidados,
indexados e independientes. La evidencia está en
`A:/nsc-fixes-evidencia/conditional-field-view.log`. El ensayo continúa sin
commit; no acredita orígenes desconocidos, llamadas que alteran campos ni alias
arbitrarios. La acumulación conservadora de nombres puede rechazar asignaciones
que una representación más precisa de rutas permitiría.

Cuando las ramas pertenecen a dos objetos locales distintos, la comprobación
de reemplazo de un objeto consultaba únicamente `viewRootLocalName`, por lo que
aceptaba sustituir el segundo objeto con su vista activa. La reproducción
`overwrite_conditional_second_root` fue solo semántica y quedó en
`A:/nsc-fixes-evidencia/conditional-second-root-before.log`. El ensayo de
`hasActiveBorrowOfOwnerLocal` consulta también las raíces de campo registradas
en la vista y confirma el ordinal de cada símbolo en el ámbito de la vista.
Tras la compilación incremental, se rechazaron el reemplazo de ambas raíces y
el movimiento de la segunda para Rc y Arc; los controles de un tercer objeto
y de reemplazo después del ámbito ejecutaron 42 en O0/O2. También pasaron los
controles anteriores de campos condicionales. La evidencia está en
`A:/nsc-fixes-evidencia/conditional-multiple-roots.log`. Esto no demuestra
la seguridad de parámetros, raíces sin identificar, alias arbitrarios ni
llamadas que modifican campos. Los cambios siguen en curso y sin commit.

Si las dos ramas llaman `Borrow()` directamente sobre propietarios move-only,
el análisis también aceptaba sustituir el segundo local o parámetro por valor.
Las reproducciones semánticas previas están en
`conditional-direct-owner-before.log` y `conditional-parameter-before.log`.
El ensayo registra las raíces de esos receptores y las comprueba con el ordinal
del parámetro, además del control ya existente para locales. Tras la compilación
incremental, la selección `shared-struct-pattern-check.cmd` rechazó reemplazar
el segundo local o parámetro y mover el parámetro, para Rc y Arc. Los controles
de un tercer propietario y del reemplazo después del ámbito ejecutaron 42 en
O0/O2; también pasaron las regresiones seleccionadas de raíces de campos y del
préstamo simple de parámetro. Evidencia: `conditional-direct-owners.log` en
`A:/nsc-fixes-evidencia`. No demuestra el tratamiento de parámetros prestados,
receptores temporales, orígenes desconocidos ni todas las formas de alias.

La reasignación de una vista exterior desde un condicional cuyo segundo dueño
está en un ámbito interior también se aceptaba; la comprobación consultaba solo
la primera raíz. La reproducción directa fue semántica y está en
`conditional-owner-scope-before.log`. En `AssignmentAnalyzer.cpp` el ensayo
comprueba las raíces registradas de las ramas del condicional contra el ámbito
de la vista antes de guardar sus fuentes. Después de compilar incrementalmente,
se rechazaron los escapes tanto de un propietario directo como de un campo para
Rc y Arc. Los controles de reasignación entre dueños del mismo ámbito y lectura
dentro del ámbito interior ejecutaron 42 en O0/O2, junto con los controles
anteriores seleccionados. Evidencia: `conditional-view-scope.log` en
`A:/nsc-fixes-evidencia`. Las raíces no identificadas y la resolución de alias
con nombres sombreados siguen sin acreditarse; el ensayo continúa sin commit.

La selección dirigida `conditional-view-alias.log` comprueba un alias de vista
en una rama condicional. Para Rc y Arc el análisis rechazó tanto una asignación
que haría sobrevivir la vista al dueño como la sustitución del dueño. Una
segunda reproducción termina el ámbito del alias antes de intentar el reemplazo
y también se rechaza: en ese caso la vista resultante debe conservar el préstamo
por sí misma. Los controles de lectura durante y después del ámbito del alias
ejecutaron 42 en O0/O2. Esta tanda no cambió el compilador y no demuestra
resolución correcta con nombres sombreados ni con orígenes desconocidos.

La selección `conditional-get-loan.log` evalúa `choose ? first.Get() :
second.Get()` para Rc y Arc. La lectura directa ejecutó 42 en O0/O2 y la
escritura directa se rechazó por no ser asignable. También se rechazó guardar
el resultado en un local sin calificador. El compilador rechaza incluso guardar
el resultado en un local `borrowed` usado solo para leer: exige una fuente
directa con dueño vivo. Por tanto, las variantes con ese local no demuestran
protección posterior del propietario; esta forma de lectura queda sin soporte
acreditado. No se cambió el compilador para aceptar el condicional ni se
ejecutaron accesos potencialmente inválidos. La prueba utilizó la compilación
incremental existente con cambios aún sin commit.

La reproducción `view_tuple_escape` asigna `(owner.Borrow(), 42)` a una tupla
declarada fuera del ámbito de `owner`. El análisis la acepta tanto para Rc como
para Arc, por lo que la prueba dirigida queda fallida; no se ejecutó el programa
ni se demostró acceso posterior al elemento. Evidencia anterior en
`view-tuple-escape-before.log` y `view-tuple-escape-arc-before.log`. El análisis
de elementos de tupla rechaza `readonlyBorrowedObject`, pero la vista prestada
de `Borrow()` no lleva esa marca. No se ha demostrado cómo preservar sus fuentes
y vidas por elemento en declaraciones, asignaciones, retornos y proyecciones.
Se consultó si debe implementarse seguimiento preciso o rechazo conservador;
la corrección permanece pendiente de esa decisión. Las reproducciones de
asignación a arreglo y campo de estructura sí fueron rechazadas para ambas
familias (`view-array-escape.log` y `view-struct-field-escape.log`), sin afirmar
que todos los agregados estén cubiertos. Todos los registros están en
`A:/nsc-fixes-evidencia`.

La variante `view_tuple_deconstruct_after_owner` añade una desestructuración
de la tupla y lectura de `view.Get()` después del ámbito del dueño. El análisis
también la acepta para Rc y Arc; no se generó ni ejecutó ese programa.
Evidencia: `view-tuple-deconstruct-rc.log` y
`view-tuple-deconstruct-arc.log`. El control `view_tuple_deconstruct_live`
mantiene al dueño vivo durante la extracción y lectura, y ejecutó 42 en O0/O2
para ambas familias (`view-tuple-deconstruct-live.log`). Este contraste muestra
que rechazar todas las vistas en tuplas también prohibiría un uso hoy funcional.

La repetición con el compilador actual y el SDK binario aislado confirmó para
Rc y Arc la aceptación semántica de cuatro rutas: salida del ámbito del dueño,
reemplazo del dueño, copia de la tupla a un local más longevo y devolución de
la tupla (`A:/nsc-fixes-evidencia/view-tuple-matrix.log`). Ninguna de esas
rutas se ejecutó. El control con dueño vivo siguió ejecutando 42 en O0/O2 con
ambas familias (`view-tuple-fixture-view_tuple_deconstruct_live-{Rc,Arc}.log`).
Las tres nuevas reproducciones se añadieron a `shared_public_pointers.py` y
fallan porque el análisis acepta cada caso; sus logs individuales empiezan con
`view-tuple-fixture-view_tuple_`. El fixture mínimo necesita traits Send y Sync
para los impl provisionales, por lo que el adaptador de evidencia los declara
solo en memoria; la primera invocación sin esa adaptación no llegó al caso.

La decisión autorizada es conservar la vida de cada vista por su origen y
rechazar conservadoramente cuando no pueda demostrarse, preservando el uso con
dueño vivo. El seguimiento existente registra raíces de vistas directas, pero
la tupla no las conserva; aún no se ha implementado la propagación para copias,
proyecciones y retornos. El defecto permanece abierto y las nuevas regresiones
no se presentan como aprobadas. Alcance y siguiente acción:
`A:/nsc-fixes-evidencia/view-tuple-lifetime-scope.md`.

El prototipo posterior reconoce por separado las tuplas que contienen vistas,
recoge sus posibles raíces al declarar y asignar un local, y aplica la
comprobación de retorno. La desestructuración propaga conservadoramente todas
las raíces de la tupla a cada vista extraída. Tras la compilación incremental,
la matriz con Core binario rechazó las cuatro rutas inseguras y aceptó la
lectura válida (`view-tuple-matrix-proto2.log`). Las regresiones dirigidas desde
fuentes para Rc y Arc también pasaron, incluidas dos raíces, tupla anidada,
copia, retorno y vista extraída que sobrevive a la tupla; los controles con
dueño vivo y dueño independiente ejecutaron 42 en O0/O2. Se conservaron los
logs `view-tuple-fixture-*-proto2.log` en A. Las comprobaciones anteriores de
escape de vista directa, arreglo y campo también pasaron.

El primer prototipo no propagaba el origen al desestructurar; se reprodujo con
`view_tuple_extracted_outlives_tuple` y se ajustó antes de la repetición. El
seguimiento no es por elemento: conserva la unión de orígenes, por lo que puede
rechazar operaciones que serían seguras con análisis más preciso. Todavía debe
revisarse la procedencia desconocida, llamadas, capturas y ramificaciones antes
de registrar el cambio o declarar cerrada la limitación. La compilación usa un
árbol con otros cambios provisionales; no certifica una revisión limpia.

Una matriz posterior descubrió que la expresión de llamada
`Identity((first.Borrow(), second.Borrow()))` perdía la segunda raíz y dejaba
reemplazar `second` en Rc y Arc. El recolector ahora examina conservadoramente
los argumentos con vistas de las llamadas que devuelven tuplas con vistas. La
misma matriz también mostró que dos ramas con dueños vivos se rechazaban por
exigir una sola raíz. Para tuplas, el ensayo retiene la primera raíz y conserva
las demás en el conjunto existente; la regla de vistas directas no cambia.

Después de compilar incrementalmente, las pruebas dirigidas rechazaron el
reemplazo de cada dueño posible y ejecutaron con 42 en O0/O2 tanto la llamada
como las ramas con dueños vivos, para Rc y Arc. También volvió a pasar el caso
de extracción (`A:/nsc-fixes-evidencia/view-tuple-fixture-*-proto4.log`). Los
resultados antes y después de la llamada están en
`view-tuple-call-matrix-before.log` y `view-tuple-call-matrix-after.log`. Un
intento de prueba de funciones libres con retorno de tupla fue rechazado por
el parser y no se cuenta como validación de esa ruta
(`view-tuple-call-matrix-namespace.log`). El prototipo sigue sin commit; faltan
comprobaciones de capturas, receptores no triviales y procedencias desconocidas.

La matriz `view-tuple-receiver-capture-before.log` mostró dos rutas adicionales
aceptadas para ambas familias: captura por copia o referencia de la tupla y
reemplazo del campo dueño de una vista devuelta por un método, con receptor
directo o anidado. No se ejecutaron los casos inseguros. El prototipo aplica a
tuplas con vistas la regla existente que rechaza capturas de vistas locales o
parámetros, y extiende la recolección conservadora del receptor a los métodos
que devuelven esas tuplas. La matriz posterior rechaza esos casos y acepta el
receptor vivo (`view-tuple-receiver-capture-after.log`).

Las regresiones `view_tuple_capture_copy`, `view_tuple_capture_ref`,
`view_tuple_capture_parameter`, `view_tuple_receiver_field` y
`view_tuple_nested_receiver_field` pasaron para Rc y Arc. Los controles
`plain_tuple_capture`, `view_tuple_receiver_live` y
`view_tuple_independent_receiver` ejecutaron 42 en O0/O2 para ambas familias;
también se repitieron los controles de llamadas y ramas (`view-tuple-fixture-*-proto5.log`).
El tratamiento del receptor es conservador: puede bloquear campos del mismo
objeto aunque el método no los preste. Los cambios siguen provisionales y no
acreditan todas las rutas de procedencia, el escape a través de otros
contenedores ni el comportamiento de bibliotecas compiladas sin esta regla.

Para comprobar una biblioteca NativeSharp anterior a esa protección se generó
un productor aislado después de retirar temporalmente solo la comprobación de
almacenamiento; enseguida se restauraron el código y el compilador, y los
controles dirigidos volvieron a pasar. El consumidor no llegó al contrato de
escape: tanto el método que almacena la tupla como el método que solo la lee
quedaron ausentes del modelo restaurado. Una segunda biblioteca, compilada con
la protección actual, confirmó que un método `Ping(int)` se restaura y que
métodos con parámetros `(int,int)` o con vista no se encuentran. Las firmas sí
aparecen en la metadata; el mapeo de tipos de la API binaria no reconoce la
clave estructural `tuple:`. Comandos, hashes y límites están en
`A:/nsc-fixes-evidencia/view-tuple-binary-escape-scope.md`. No se ejecutó la
posible fuga. La restauración de firmas de tupla y, después, el contrato de
escape entre módulos quedan pendientes; no se consideran probados por este
rechazo de resolución.

La matriz `view-tuple-aggregate-matrix-before.log` identificó un escape por
un inicializador: una estructura por valor contenía la tupla y se devolvía
después de destruir el dueño. El ensayo recorre estructuras por valor y
tuplas anidadas para reconocer ese préstamo, sin seguir referencias de clase;
también recorre los inicializadores de agregados. El retorno, el reemplazo del
dueño, la copia a un ámbito exterior y el retorno de una estructura anidada se
rechazaron para Rc y Arc. Un control de estructura con tupla ordinaria ejecutó
42 en O0/O2 (`view-tuple-aggregate-rejects.log`).

El control válido de estructura con vista ejecutó 42 en O0/O2 para ambas
familias al consumir el Core binario (`view-tuple-binary-aggregate-live-before.log`).
Sin embargo, la selección equivalente con Core desde fuentes falló al emitir
el ejecutable: `tuple field layout could not be computed` para `SharedView`;
el análisis semántico sí lo había aceptado (`view-tuple-storage-directed-proto7.log`).
Los controles mínimos de estructuras genéricas, con argumentos incorporados
o de clase, declaradas en el mismo archivo o en archivos separados, compilaron
y ejecutaron 42 (`tuple-struct-layout-probe-{generic,separate,nested,class}.log`).
Esto limita la reproducción al fixture más complejo: no se ha demostrado la
causa ni modificado el cálculo compartido de layout. El prototipo y la prueba
dirigida completa siguen pendientes; no se afirma una solución terminada.

Actualización: el fallo de layout se redujo a un elemento de estructura cuyo
nombre se procesa después de la estructura que contiene la tupla. Se corrigió
en el commit local `e1b66c188`; la selección de vistas desde fuentes pasó después
(`view-tuple-storage-directed-restored.log`), y el control válido de estructura
ejecutó 42 en O0/O2 para Rc y Arc. La prueba unitaria vecina de layout tuvo
tres fallos que se reprodujeron con y sin ese cambio; después se corrigieron
sus fixtures y pasaron las 47 comprobaciones. Los detalles están en
`docs/fixes/tuple-field-layout-dependencies.md`. El seguimiento de vistas y sus
restantes rutas siguen provisionales.

El fixture aislado de la prueba del proyecto declara ahora los traits Send y
Sync necesarios para analizar las fuentes provisionales de Arc; las
reproducciones originales se conservan. La selección dirigida de 28 casos por
familia pasó mediante `nsc.sdk.shared-public-pointers` (1/1) con Rc y Arc,
incluidos los controles de ejecución O0/O2 y los rechazos esperados
(`A:/nsc-fixes-evidencia/view-tuple-directed-ctest.log`). No se ejecutó la
batería completa. Esta pasada usa el árbol local conjunto y no cierra por sí
sola la revisión del prototipo.

La matriz `view-tuple-storage-matrix-before.log` también aceptó almacenar la
tupla con vista en un campo, directamente o dentro de un método que la recibe
como parámetro. El ejemplo de arreglo fue rechazado por sintaxis, antes del
análisis de préstamos, y no acredita protección de arreglos. El prototipo
rechaza conservadoramente asignar una tupla con vista fuera de un local no
estático rastreado; esto incluye campos y reasignaciones de parámetros, aun
cuando alguna asignación pudiera ser segura con un contrato de vida más
preciso. Tras la compilación incremental, ambas rutas de campo se rechazaron
con Core binario y se aceptó la lectura por parámetro
(`view-tuple-storage-matrix-after.log`).

La selección `view-tuple-storage-directed.log` pasó 1/1 para Rc y Arc: rechaza
las dos rutas de campo y ejecuta 42 en O0/O2 para lectura por parámetro, tupla
ordinaria en un campo y los controles de vista viva, llamada y receptor. El
recolector no acredita aún métodos precompilados que reciban y almacenen una
tupla, setters ni inicializadores de agregados; la protección sigue siendo un
prototipo sin commit.

Una vista de un propietario exterior impedía mover o reemplazar otro local
distinto con el mismo nombre en un ámbito interior. La reproducción del rechazo
está en `shadow-owner-before.log`. `hasActiveBorrowOfOwnerLocal` comparaba el
nombre de la raíz sin verificar su símbolo; el ensayo resuelve ahora la raíz
en el ámbito de la vista y compara el ordinal del local. La compilación
incremental pasó; los controles con propietario interior distinto ejecutaron
42 en O0/O2 para Rc y Arc, y los casos de dueño interior o exterior realmente
prestado siguieron rechazados. También pasaron los controles seleccionados de
vistas condicionales y alias. Evidencia: `shadow-owner-identity.log` en
`A:/nsc-fixes-evidencia`. Esto no acredita todas las combinaciones de sombra
en fuentes de campos ni todas las formas de alias; el cambio sigue sin commit.

Los controles `shadow_unrelated_owner_field`,
`shadow_inner_owner_field_borrowed` y `shadow_outer_owner_field_borrowed`
comprueban campos `value` de dos objetos locales con el mismo nombre. El campo
independiente se reemplazó y la lectura exterior ejecutó 42 en O0/O2; los
reemplazos de los campos realmente prestados se rechazaron para Rc y Arc.
Evidencia: `A:/nsc-fixes-evidencia/shadow-field-identity.log`. La selección
utilizó el compilador incremental previo, sin cambios nuevos de producto; no
acredita alias arbitrarios ni resolución de nombres en todos los ámbitos.
Para cada familia, el análisis rechaza guardar `Borrow()` de un propietario
temporal en una vista, y rechaza guardar el resultado de `Get()` tanto en un
local `borrowed` como en uno sin calificador. Los diagnósticos son distintos:
receptor temporal, falta de dueño vivo y pérdida del préstamo, respectivamente
(`public-temporary-owner-view.log`, `public-temporary-owner-get.log` y
`public-temporary-owner-get-unqualified.log`). Son controles de análisis sobre
esas expresiones, no una prueba de todas las expresiones temporales.

La construcción incremental de `NativeSharpCore.lib` falló al verificar la
metadata: una plantilla genérica pública alcanza un helper interno cuyo tipo
propietario no está en la metadata (`public-core-build.log`). La reproducción
mínima demuestra fallo con propietario interno y éxito con propietario público
(`metadata-internal-generic-probe/results.json`); cambiar la visibilidad del
helper sería una alteración de API y no se aplicó. Se consultó al usuario sobre
el archivo de metadata compartido con interoperabilidad antes de modificarlo.
Como control adicional, una copia de la biblioteca mínima con propietario
público se consumió mediante `LibraryReference` desde otro proyecto NativeSharp:
compiló y ejecutó con resultado 42 en O0 y O2
(`metadata-generic-consumer-probe/results.json`). El control demuestra ese
recorrido binario público; no demuestra que el caso interno esté corregido ni
que Core pueda distribuirse todavía como binario.

La barrera provisional de llamadas binarias con tuplas que contienen vistas
omitía las funciones de namespace: con un mapeo temporal de diagnóstico,
`Helpers.Read` pasaba el análisis mientras el método equivalente `Reader.Read`
recibía NS3020, tanto con Rc como con Arc. El productor era un lector seguro y
no se ejecutó ningún consumidor. En el árbol local se consulta ahora el origen
precompilado de la función de namespace antes de aplicar la misma barrera. Las
cuatro llamadas recibieron NS3020 en el ensayo; el mapeo temporal se retiró y
el compilador se reconstruyó incrementalmente. Los controles dirigidos de 13
escenarios por familia pasaron. Detalles, comandos y limitaciones en
`A:/nsc-fixes-evidencia/namespace-view-tuple-scope.md`. Esta barrera también
rechaza lecturas seguras y sigue siendo provisional: las firmas binarias con
vistas compuestas y un contrato fiable de bibliotecas previas siguen pendientes.
Se preparó `tests/integration/generic_internal_owner_metadata.py` para exigir
la producción y consumo binario con helper interno y comprobar que el
consumidor no pueda nombrarlo. Actualmente falla en la producción de metadata
por el diagnóstico del propietario ausente
(`internal-owner-metadata-regression.log`); aún no alcanza las otras etapas.

Un contraste adicional colocó un helper genérico `private`, y luego `internal`,
dentro de una clase ya pública. La producción de la biblioteca y el consumo de
su método público pasaron con salida 42 en O0/O2; la invocación directa del
helper desde el consumidor fue rechazada en ambos casos
(`A:/nsc-fixes-evidencia/public-owner-private-generic/results.json` y
`access-results.json`). Esto prueba esa estructura mínima, no el SDK completo.
Mover simplemente llamadas públicas a un wrapper no elimina las dependencias
transitivas de helpers cuyos propietarios son internos. Además,
`SharedControlStorage` y `SharedStorage` participan en reconocimientos especiales
del backend, por lo que mover sus métodos exigiría adaptar esos reconocimientos
y verificar sus consumidores. No se aplicó esa reorganización ni se presentó
como solución binaria; el cambio de metadata compartida sigue pendiente de la
autorización ya consultada.

Un control independiente descubrió un problema anterior a la corrección de
metadata: con `ProjectReference` a fuentes, un consumidor accedió a un método
público de un tipo declarado `internal` en otro proyecto; compiló y terminó
con 42 (`internal-owner-visibility-probe/result.json`). El contraste semántico
aceptó tanto un método ordinario como uno genérico del tipo interno, mientras
rechazó un método `internal` de un tipo público y una declaración local con
el mismo tipo interno. El acceso estático también se aceptó al quitar el
`using` del namespace del productor
(`internal-visibility-contrast/result.json`). El contrato de visibilidad de
`spec/NativeSharp_Language_Specification_1_0.md`, sección NS-NAME-001, indica
que `internal` es del proyecto y que la metadata no debe ampliar acceso.
`resolveTypeIdentifier` contiene una búsqueda alternativa por nombre que no
consulta `typeIsVisibleFromProject`. Se añadió la comprobación al receptor de
clase `internal` obtenido por esa vía; las regresiones de fuentes, Friend y
API del productor pasaron. Detalles y límites en
`visibilidad-tipo-interno-estatico.md`. El caso binario con el helper interno
aún no se puede ejecutar, por lo que su visibilidad no se da por verificada.

La prueba `public-pointers-send-probe/results.json` mostró que `Rc<Safe>` y
`WeakRc<Safe>` no satisfacen `Send` ni `Sync`, como corresponde a referencias
locales, pero `Arc<Safe>` y `WeakArc<Safe>` tampoco los satisfacen. Por ello
las familias atómicas todavía no cumplen su contrato de uso entre hilos;
haber ejecutado contadores atómicos mediante un puntero de prueba no demuestra
la transferencia segura de `Arc`.

Para una futura comprobación condicionada, el modelo genérico conserva la
clave de la plantilla y los argumentos de cada especialización en
`GenericSpecializationRecord`; `ProjectGenericModel` ofrece
`specializationForTypeKey`. El análisis de cuerpos dispone de `genericModel_`.
La comprobación de constraints recibe el modelo genérico en `build`, pero
`satisfiesThreadSafetyConstraint` y sus llamadores actuales no lo reciben.
Esto identifica información y puntos de integración posibles; aún no demuestra
que la identidad del proveedor sea autenticable ni que el argumento conservado
se resuelva correctamente en todos los casos precompilados. No se ha relajado
el rechazo de `Arc` ni se ha introducido una excepción por nombre.
La implementación actual de `specializationForTypeKey` retira el prefijo de
tipo y busca el nombre terminal en el índice, sin contrastar allí el módulo o
namespace completos (`src/Frontend/ProjectGenericModel.cpp`). Por tanto, una
futura regla de seguridad no puede tratar el resultado de esa búsqueda por sí
solo como prueba de procedencia: debe contrastar la identidad canónica y su
origen antes de aplicar el tratamiento especial. Esta inspección no demuestra
por sí sola que exista una colisión explotable en la configuración de Core.

La reproducción `arc-constructor-alias-probe/result.json` encontró otra
condición relevante para clases: un constructor asigna `this` a un campo
estático, y el análisis acepta después una escritura por ese alias mientras
existe `Arc<Payload>`. En O0 y O2, la lectura por `owner.Get()` devuelve 9,
el valor escrito por el alias global. Esto demuestra que el solo lectura de
`Get()` no elimina referencias mutables preexistentes ni prueba exclusividad
del objeto; no se ejecutó un acceso después de destruirlo ni una carrera.
Antes de habilitar transferencia se consultó al usuario si debe exigirse
demostrar que el constructor no hace escapar un alias mutable. La prueba
semántica `shared-constructor-escape-probe/results.json` aceptó el acceso al
alias después del alcance del propietario para ambas familias fuertes, tanto
si el alias se registra en el constructor como en el destructor. No se
ejecutó ese acceso potencialmente inválido. Se consultó por separado si la
construcción debe exigir esa prueba también para `Rc` y para el uso local de
`Arc`; la respuesta sigue pendiente. Una prueba limitada al constructor no
cubriría la publicación en el destructor, por lo que sigue abierta la
definición de una garantía completa de vida del objeto.
El análisis actual distingue `CurrentReceiver` como origen, pero
`AssignmentAnalyzer.cpp` registra escapes de asignación para parámetros con
ordinal y no registra por esa ruta la publicación del receptor. Esta
inspección explica una limitación del registro observado; no demuestra que
corregir solo esa asignación cubra llamadas, alias intermedios, destructores o
implementaciones precompiladas.

Una segunda reproducción usa `TaskScope<ThreadExec>.Spawn` y una captura
`ref` de los tipos públicos reales de Core. El análisis rechazó las seis
combinaciones `Rc`, `Arc` y `WeakArc` con `Safe` y con `Raw` (este último
contiene un puntero crudo), con el diagnóstico de captura no `Send`. Las
fuentes exactas, comandos y diagnósticos están en
`A:/nsc-fixes-evidencia/arc-capture-probe/results.json`. El rechazo de
`Rc` y de los valores con `Raw` es esperado; el de `Arc<Safe>` y
`WeakArc<Safe>` confirma que la limitación afecta también a las capturas del
ejecutor, además de las restricciones genéricas. Esta prueba comprueba
análisis semántico, no ejecución concurrente.

La prueba dirigida de retención y liberación concurrentes del control interno
pasó en O0/O2; comprueba la prohibición de resucitar tras observar expiración.
Su diseño, resultado y límites están en `shared-atomic-upgrade.md`. No habilita
el envío entre hilos de las familias públicas ni sustituye esa validación.

El conteo débil atómico durante destrucción reentrante se corrigió y verificó
por separado; el detalle, las pruebas y sus límites están en
`shared-atomic-reentrant-count.md`. La corrección no resuelve la transferencia
entre hilos de la API pública.

La prueba `nsc.sdk.shared-public-atomic-race` usa los tipos públicos desde
fuentes: un `WeakArc` estático permanece estable hasta `Thread.Join`, y un hilo
secundario realiza un upgrade exitoso antes de una fase que compite con la
liberación del fuerte principal. En 128 rondas de 64 intentos comprueba el
payload de los upgrades exitosos, ausencia de resurrección tras observar
expiración, un destructor y el débil explícito restante. Pasó en O0/O2
(`public-atomic-race-ctest.log`). La planificación no garantiza que cada ronda
realice operaciones simultáneas; el acceso al estático evita el análisis de
captura y no acredita el contrato `Send`/`Sync` ni el consumo binario.
La variante `nsc.sdk.shared-public-weak-clone-race` añade clones temporales de
`WeakArc` en ambos hilos y hace los upgrades del worker desde esos clones. Pasó
en O0/O2 (`A:/nsc-fixes-evidencia/shared-weak-clone-race.log`), verificando al
final la expiración, un destructor por ronda y un débil explícito restante.
La planificación no garantiza un intercalado concreto ni prueba todas las
carreras; tampoco habilita la transferencia pública entre hilos.
La regresión `nsc.sdk.shared-public-last-strong-weak` coordina el orden: el
worker retiene un clon débil y espera; el hilo principal libera el último
fuerte; solo entonces el worker intenta `Upgrade`. Durante 128 rondas verifica
los dos débiles antes de la liberación, destrucción única, expiración, upgrade
vacío y un débil tras el join. Pasó en O0/O2
(`A:/nsc-fixes-evidencia/shared-last-strong-weak.log`). Este orden coordinado
es evidencia concreta, pero no prueba todos los intercalados ni Send/Sync.

Una variante sustituye los tipos por `Rc` y `WeakRc`, conserva ese método de
hilo y un campo estático, y solo solicita análisis semántico. El compilador lo
acepta (`public-local-thread-negative.log` registra el fallo de la expectativa
de rechazo). No se ejecutó la variante, porque usa contadores no atómicos.
`Thread.ns` documenta que `ThreadStart` acepta métodos estáticos sin una
comprobación interprocedural de sus accesos. El usuario eligió prohibir el
almacenamiento estático compartido de `Rc`/`WeakRc`, incluidos contenedores;
la validación rechaza ahora ese caso y la regresión semántica pasó
(`public-local-thread-after.log`). El diseño, los controles y las limitaciones
de módulos precompilados están en `shared-static-storage.md`.

Otras dos reproducciones aceptaron copiar un `Rc` o `Arc` dentro de una tupla,
tanto con `(owner, 42)` como con `{ owner, 42 }`. Solo se ejecutó el análisis
semántico, no el programa que podría liberar dos veces. Los cuatro resultados
anteriores a una corrección están en los archivos
`public-tuple-borrow-tuple_owner_copy,tuple_owner_aggregate_copy.log`,
`public-tuple-borrow-tuple_owner_copy-Arc.log` y
`public-tuple-borrow-tuple_owner_aggregate_copy-{Rc,Arc}.log`. La corrección y
sus límites sobre `move` están en `tuple-shared-copy.md`. El condicional que
ocultaba otra copia se corrige por separado en `conditional-move-only-copy.md`.
Sigue pendiente la limpieza de un clon en una tupla parcialmente inicializada:
si el segundo elemento lanza, la excepción se captura pero el contador fuerte
queda en 2 en vez de 1, para `Rc` y `Arc`, con ambas sintaxis en O0 y O2
(`tuple-partial-cleanup-probe/results.json`). Un control con la misma tupla y
`try` que termina normalmente sí libera el clon en ambas sintaxis y niveles
(`public-tuple-borrow-tuple*try_success-{Rc,Arc}.log`). El LLVM del caso que
lanza almacena el clon antes de invocar el segundo elemento, pero solo marca
viva la tupla completa después de ese invoke; el unwind va directamente al
catch sin limpiar el primer elemento
(`tuple-partial-cleanup-probe/arc-aggregate-llvm.log`).

La comprobación de restricciones usa
`ProjectGenericConstraintModel::satisfiesThreadSafetyConstraint`, mientras
que las capturas pasan por `ProjectMethodBodyModel::callTypeIsSend` y
`callTypeIsSync`. Las dos rutas recorren los campos y llegan al puntero crudo
interno del control atómico. Cualquier tratamiento especial debe acreditar
la identidad de la implementación de Core y exigir que el argumento cumpla
los contratos necesarios, además de conservar el rechazo de los controles
locales y de valores inseguros en ambas rutas. El modelo de hechos
`ThreadSafetyTypeFacts` contiene campos para una prueba insegura auditada,
pero no se ha comprobado que exista una superficie de lenguaje que la active;
su presencia aislada no autoriza considerar `Arc` seguro.

Como control adicional, `thread-payload-constraints-probe/results.json`
registra diez análisis de restricciones directamente sobre las cargas: `Safe`
satisface ambas; `Raw`, ninguna; y `SendOnly` (con `ref mut int`) satisface
`Send`, pero no `Sync`. Una estructura `RefMutRaw` con `ref mut Raw` también
fue aceptada para `Send` y rechazada para `Sync`. Esto muestra que comprobar
solo `Send` sería insuficiente para la regla de `Arc`. No se ejecutó ni se
demostró una carrera con esos tipos; el resultado es exclusivamente del
análisis semántico actual.

El manifiesto de `src/Language/SmartPointerSemantics.cpp` exige mantener
disponible el módulo productor mientras exista el bloque. La búsqueda de
`ModuleLifetimeRegistry` en `src` e `include` solo encontró su declaración y
su implementación en `CrossModuleLifetime.cpp`; no identificó una llamada
desde las fábricas o los controles compartidos. El registro es un modelo en
C++ del compilador y su mera existencia no acredita retención en el runtime.
Sigue sin verificarse la descarga real de un módulo mientras sobreviven
referencias fuertes o débiles.

La documentación `docs/3.0.4/items/NS304-T16-F04-dll-ownership.md` cita
`TL-LANG-509` para ownership entre EXE y DLL, pero el cuerpo de esa prueba en
`tests/compiler-suite/host/modules/LanguageSurfaceModule.cpp` construye y
ejecuta un consumidor de una DLL sin solicitar su descarga mientras el objeto
está vivo. Por tanto, esa prueba no demuestra el requisito de retener el módulo
hasta liberar el último débil. Tampoco se encontró `FreeLibrary` en
`src/Runtime` ni `lib/NativeSharp.Core` en esta búsqueda; esto no demuestra que
una descarga externa sea imposible. La verificación de retención sigue abierta.
### Rc y Arc en finally suspendido durante una excepción

En la revisión `46a58d434`, `A:/nsc-fixes-evidencia/shared-finally-current.cmd`
compiló desde fuentes Rc y Arc en O0 y O2. El future recibe un clon del dueño;
su `try` lanza y el `finally` construye una tupla con otro clon y un `await`
que suspende. Tras esperar el future, la salida codifica
`10 + 10*caught + StrongCount`. Las cuatro ejecuciones devolvieron 12: la
excepción no llegó al catch y se observaron dos fuertes en vez de uno. No se
determinó cuál de los dos clones quedó retenido ni se inspeccionó memoria liberada.

`shared-finally-controls.cmd` repite las mismas familias y optimizaciones.
Si el await completa inmediatamente durante la excepción, los cuatro casos
devuelven 21: excepción capturada y un fuerte. Si el try termina normalmente
y el await suspende, devuelven 11: sin excepción y un fuerte. Comandos,
fuentes, diagnósticos y resultados están en `shared-finally-current/`,
`shared-finally-control-ready/` y `shared-finally-control-normal/` dentro de
`A:/nsc-fixes-evidencia`. La primera versión del control normal no compiló por
falta de retorno; se agregó un retorno y se repitió antes de registrar los
resultados anteriores. El compilador usa el build incremental con cambios
concurrentes; estos ensayos usan fuentes, no el SDK binario.

La reproducción añade evidencia de impacto en los punteros públicos al fallo
de continuación ya conocido. No demuestra la causa exacta de la retención y
no se considera una corrección. El intento previo de identificadores de
reanudación produjo LLVM inválido en finally anidados: se requiere una solución
que conserve continuaciones y respete los funclets, con sus pruebas dirigidas.

En `shared-finally-counts/`, el contador es 2 antes de construir la tupla y 3
después de completar su await, en ambas familias y optimizaciones. El contraste
`shared-finally-no-tuple-clone/` sustituye el clon de la tupla por un entero;
la excepción sigue sin capturarse y el contador final sigue siendo 2. Esta
comparación muestra que la retención no necesita el clon de la tupla, pero no
identifica por sí sola la instrucción de limpieza omitida.

La regresión `tests/integration/shared_async_finally_owner.py` conserva las
dos formas con excepción y suspensión y los dos controles por familia en O0/O2.
En `shared-finally-regression-before.log` fallan las ocho ejecuciones con
excepción suspendida (salida 12 frente a 21 esperada) y pasan las otras ocho.
La prueba conserva el resultado fallido; aún no está registrada en CTest ni
acredita la corrección.

La misma prueba se amplió para recibir el SDK binario aislado
`shared-observer-sdk/NativeSharpCore.lib`. Con un constructor explícitamente
público en el objeto de prueba, las variantes con excepción y clon en la tupla
no llegan a ejecutarse: el backend termina con `LLVM ERROR: Cleanup funclets
for the SEH personality cannot contain exceptional actions`, tanto con await
listo como suspendido y para ambas familias y optimizaciones. Sin el clon de
tupla, los cuatro casos de excepción suspendida compilan y devuelven 12 en vez
de 21; los cuatro controles de salida normal devuelven 11. El registro es
`shared-finally-regression-binary-public-ctor.log`. La prueba desde fuentes
con el mismo constructor conserva los ocho fallos y ocho controles aprobados
(`shared-finally-regression-source-public-ctor.log`). El primer intento binario,
con constructor implícito, no pasó análisis de accesibilidad y se conserva en
`shared-finally-regression-binary-before.log`. El SDK binario se reutilizó de
un build con cambios concurrentes, por lo que esta comparación no certifica
un SDK reconstruido de un árbol limpio. Se conserva la reproducción y no se
atribuye todavía el error LLVM a una causa específica.

El contraste `shared-finally-explicit-exception.cmd` sustituyó la excepción
con constructor implícito por una excepción con constructor explícito vacío,
conservando el SDK desde fuentes. Las cuatro compilaciones de Rc/Arc en O0/O2
terminaron con el mismo error de funclet. La regresión dirigida incorpora esa
variante; en `shared-finally-regression-source-explicit-exception.log` se
conservan sus cuatro fallos de compilación, los ocho fallos de ejecución
anteriores y los ocho controles aprobados. Por tanto, el fallo de compilación
no es exclusivo del SDK binario.

El LLVM de la variante binaria está en `shared-finally-binary-ir/llvm.log`.
El constructor explícito de excepción se emite como `invoke` con destino de
unwind a `eh.cleanup.1.12`. Ese bloque crea `eh.cleanuppad1` y baja el cuerpo
del finally; dentro, la llamada a `Arc.Clone()` se emite como otro `invoke`
con el bundle de ese funclet y un destino de unwind externo. Esta forma es la
acción excepcional presente en el funclet que LLVM rechaza. La comparación
del caso desde fuentes con excepción implícita no genera esa misma ruta para
su construcción. Evitar sin evidencia el `invoke` o declarar que `Clone` no
puede lanzar ocultaría el problema de excepciones dentro de la limpieza; la
solución debe manejar tanto esa salida como la reanudación fuera del funclet.

El LLVM de la variante Arc sin clon de tupla se guardó en
`shared-finally-no-tuple-clone/llvm.log`. En `Work$poll`, el despacho del estado
1 entra a `coro.resume.stmt.25`, dentro de la copia normal de `finally`; al
terminar el await llega a `eh.try.cont.7`, que publica resultado 0 y estado -1
sin destruir el parámetro propietario. En el `$drop` emitido, la limpieza del
parámetro solo aparece en la ruta de future no iniciado, estado 0. Esta
inspección explica la observación de la variante medida y no acredita el resto
de rutas de liberación del frame ni una solución para excepciones anidadas.

### Alcance observado de la limpieza de tuplas (investigación)

Actualización de contraste en `3e3d83d30`: el prototipo mencionado a
continuación ya está incluido en el punto de control `57c47c297`; la descripción
histórica «sin commit» corresponde al momento del ensayo. Tras compilar de
forma incremental el objetivo `nsc`, la regresión dirigida
`tests/integration/tuple_partial_initialization.py` devolvió 42 en sus 28
ejecuciones O0/O2. Con las fuentes actuales de Rc y Arc, el control de
construcción interrumpida después del clon y el de salida normal devolvieron
contador 1 en O0 para ambas familias. Los comandos y resultados de estos cuatro
controles están en `A:/nsc-fixes-evidencia/tuple-partial-cleanup-counts-current/`.
El compilador de integración produjo los mismos cuatro resultados tras configurar
la ruta de bibliotecas de importación en A; sus registros están en
`tuple-partial-cleanup-counts-integration/`.

Estos controles no resuelven los casos de suspensión, cancelación, transferencia
o doble excepción descritos más abajo. El build de trabajo incluye archivos de
otro trabajo local y el de integración reutiliza objetos anteriores; los
resultados no constituyen certificación de un árbol limpio ni del SDK binario.

En la revisión `cbedd8f04`, `A:/nsc-fixes-evidencia/tuple-partial-cleanup-boundaries.cmd`
compiló desde fuentes las familias `Rc` y `Arc` con O0 y O2. El programa observa el
contador fuerte después del ámbito de la tupla. Si el primer elemento lanza antes de
clonar, el contador es 1; si lanza después de un clon es 2; después de dos clones es 3;
y si la tupla interior se completó antes de fallar el elemento exterior es 2. Los
registros están en `A:/nsc-fixes-evidencia/tuple-partial-cleanup-counts/results.json`;
el código de salida es 10 más el contador. Todos esos valores se repiten en ambas
familias y optimizaciones. La excepción se comprueba antes de leer el contador.

También se observó contador 2 tras salir **normalmente** del ámbito de una tupla
`(owner.Clone(), 42)`. Este control adicional muestra que la vía normal y la
excepcional necesitan investigación separada; el control anterior solo comprobaba
la construcción dentro del ámbito y no demostraba su destrucción. En el IR ya
conservado `tuple-partial-cleanup-probe/arc-aggregate-llvm.log`, el bloque normal
`value.local.destroy.9` desactiva la marca y salta sin destruir un elemento.
`appendTupleLayout` registra las tuplas con ordinal de tipo sintético máximo y los
ordinales de sus campos son índices de elemento, no miembros nominales.
`emitValueDropAddress` deriva la limpieza al tratamiento de miembros nominales,
que consulta esos ordinales como miembros del proyecto. La ruta necesita una
destrucción estructural de elementos vivos, en orden inverso, sin interpretar esos
índices como símbolos nominales. Esto no resuelve por sí solo la construcción
parcial: allí además se necesita conocer qué elementos llegaron a inicializarse.
La salida normal se corrigió después en `dfd3dcdc2`; el detalle y las pruebas
están en `tuple-value-drop.md`. La construcción interrumpida permanece sin
corregir, y no se ha verificado aquí la semántica de una segunda excepción durante
cleanup.

La reproducción independiente `A:/nsc-fixes-evidencia/tuple-partial-guard-probe.py`
usa estructuras `Guard` cuyos destructores registran sus identificadores. Con
`Gate.Read()` fallando antes del primer elemento, el resultado 0 es correcto;
después de construir un elemento debería ser 1 y después de dos debería ser 21
por orden inverso. Tanto en declaraciones como al construir una tupla argumento,
los casos de uno y dos elementos devuelven 0 en O0 y O2. También devuelve 0 la
tupla exterior que contiene una tupla interior de dos elementos ya completada.
La excepción se comprueba en cada caso. El resultado completo y los comandos
están en `tuple-partial-guard-probe/results.json` y sus registros individuales.
Esta prueba separa la omisión de destructores del estado de `Rc`/`Arc` y muestra
que la ruta de valores preevaluados también necesita tratamiento. No demuestra
qué debe ocurrir si uno de esos destructores lanza durante la propagación.
El volcado `tuple-partial-guard-probe/argument-llvm.log` confirma que los dos
`Guard` se construyen antes de `Gate.Read`; sus resultados solo se almacenan en
la tupla después de que `Read` retorna. El unwind de los tres `invoke` salta
directamente al mismo catch, sin una limpieza de los valores ya construidos.

Un prototipo aún sin commit introduce regiones temporales para elementos de
tupla ya construidos y usa esa construcción para expresiones anidadas. Las 20
ejecuciones dirigidas de `tuple-partial-regression-prototype.log` pasaron en
O0/O2, incluyendo constructor fallido, orden inverso y argumento; los 20 cruces
de Rc/Arc en `tuple-partial-cleanup-counts-prototype/results.json` conservaron
contador 1 después del ámbito o excepción. Esto no certifica el prototipo: faltan
la revisión de profundidad de lowering, casos con spread y rutas de transferencia,
así como las pruebas de regiones anidadas y los controles relacionados. Sigue
sin establecerse la semántica de doble excepción.

En `tuple-partial-regression-regions.log` pasaron además los casos con una
excepción capturada dentro del método llamado y con dos iteraciones que crean,
destruyen y capturan por separado. Los tres controles de tuplas y agregados y
`nsc.compiler.tuple-value-drop` volvieron a pasar. Una prueba O0 de anidamiento
completó 4, 8, 16 y 32 niveles con todos los destructores esperados; a 64 el
frontend rechazó la profundidad de inferencia antes del lowering, con el mismo
diagnóstico en el compilador base. Evidencia en `tuple-partial-depth-probe/`.
Esta medición no prueba que la ruta recursiva sea segura para toda expresión
admitida ni cubre spread, corrutinas o transferencia por rutas distintas.

La ampliación `tuple-partial-regression-finally.log` pasó en O0/O2 cuando la
excepción sale del método que construye la tupla y cuando un `finally` exterior
debe correr después de los destructores, en orden 2, 1, 3. El análisis dirigido
`tuple-spread-custom-probe-only/result.log` rechaza la forma concreta de
`..move(source)` sobre `(Guard, int)` por el destructor transitivo. Esto delimita
esa forma, pero no demuestra que ninguna otra expansión o corrutina pueda llegar
al lowering; ambas permanecen sin cobertura exhaustiva.

Una prueba async independiente en `tuple-partial-await-probe/` construye un
`Guard` como primer elemento y espera un productor que suspende en el segundo.
La salida 21 en O0/O2 indica dos construcciones y una destrucción; se esperaba
42, correspondiente a una construcción y una destrucción. La misma fuente con
el compilador de base en `tuple-partial-await-baseline-probe/` devuelve 20 en
ambos niveles: también construye dos veces y no destruye el elemento. La doble
construcción es anterior al prototipo; la diferencia de destrucción no permite
atribuirse exclusivamente a él porque `dfd3dcdc2` cambió el drop de tuplas.
La prueba no verifica cancelación ni excepción durante la suspensión. Esta ruta
permanece abierta; no se declara que el prototipo sea seguro a través de await.

El MIR de `tuple-partial-await-probe/current-mir.log` contiene un
`aggregate-element-store` para `new Guard()` como hermano del elemento que
contiene `await`. El selector `assignReplaySpillSlotsTo` en
`MirFunctionLowerer.cpp` excluye `AggregateElementStore` como candidato y solo
desciende por direcciones de elementos de arreglo; el recorrido correspondiente
`markCoroutineReplayOperands` en `OopLlvmCoroutines.cpp` también solo desciende
por esas direcciones. Por tanto, el constructor dentro del wrapper no obtiene ni
registra por esa vía un slot de repetición. Esta es la causa candidata de la
segunda construcción, no una corrección todavía: conservar un valor con drop en
el frame también requiere acreditar su destrucción si el futuro se abandona y
transferir su propiedad una sola vez al completar la tupla.

Un ensayo provisional desciende por el wrapper del elemento de tupla en ambos
recorridos. La reproducción `tuple-partial-await-replay-prototype/results.json`
devuelve 42 en O0/O2: en esa ejecución el constructor y destructor se invocan
una vez cada uno. La regresión dirigida de excepciones de tupla también pasó
(`tuple-partial-regression-replay-prototype.log`). Esto confirma la hipótesis
para el caso medido, pero no la seguridad de ownership guardado en el frame al
abandonar o cancelar el futuro; el ensayo no está comprometido ni registrado
como corrección terminada y permanece pendiente esa verificación.

El LLVM del ensayo, `tuple-partial-await-replay-prototype/llvm.log`, guarda el
valor `Guard` en el frame en offset 40. La función de drop del futuro `Main`
generada solo libera el frame cuando el estado es 0; para otro estado retorna sin
emitir el destructor de ese slot. Esta observación estática no demuestra por sí
sola qué ocurre en todas las vías de cancelación ni que ese drop sea invocado.
El comentario de `lowerFutureDrop` indica que el diseño actual considera que
un future iniciado no puede abandonarse suspendido desde código fuente, porque
se consulta mediante `await` hasta completar. Esa afirmación es un contrato de
implementación, no una comprobación de todas las rutas de generadores async o
cancelación. No se ha encontrado aquí una API pública de poll para reproducir
directamente el abandono de un future suspendido.
Una prueba diferente sí cubre el contrato aprobado del iterador async: el
generador construye la tupla alrededor de un `await` que suspende, termina la
tupla y produce un elemento; después `Cancel()` no destruye inmediatamente y el
siguiente `MoveNextAsync()` destruye el `Guard` una sola vez. El ensayo pasa O0/O2
con salida 42 (`tuple-async-generator-cancel/results.json` y
`tuple-await-cancel-test.log`); la base devuelve 30 al detectar dos
construcciones antes de cancelar. La regresión en preparación está en
`tests/integration/tuple_await_cancel.py`. Esta prueba cancela después de que el
`await` terminó y se produjo el elemento; no prueba cancelar durante el await.

Las pruebas dirigidas de `tuple-await-replay-test.log` pasaron en O0 y O2 para
dos suspensiones en una sola tupla: los dos `Guard` se construyen una vez cada
uno, se destruyen en orden inverso y la expresión posterior corre una vez. También
pasó una excepción capturada en un elemento evaluado después de completar el
await: el `Guard` anterior se construye y destruye una vez. Se conserva la
regresión en preparación `tests/integration/tuple_await_replay.py`. Ninguno de
estos casos prueba excepción dentro del productor suspendido ni cancelación
durante el await; esos límites permanecen abiertos.

El 9 de noviembre se añadieron dos controles dirigidos a esa regresión: una
tupla anidada con `await` y una tupla construida en dos iteraciones de un
bucle. `A:/nsc-fixes-evidencia/tuple-await-nested-loop.cmd` compiló y ejecutó
ambos controles en O0 y O2; las cuatro ejecuciones devolvieron 42 y verificaron
que cada `Guard` se construyó y destruyó una vez por iteración. Es evidencia
del prototipo actual, no una corrección registrada ni una prueba de cancelación
durante la suspensión.

La prueba `tuple-public-async-cancel/results.json` ejecutó desde fuentes las
cuatro familias públicas en O0/O2. Una tupla con clon fuerte o débil se completa
después de await; los conteos esperados se observan antes de producir un elemento
y, tras cancelar y avanzar, el payload se destruye una vez. No mide directamente
la liberación final del bloque débil ni acredita el consumo binario.

Al ampliar la prueba con un débil observador en el llamador y transferir el fuerte
al generador, las ocho variantes dejaron de cumplir los conteos esperados antes
de cancelar (`tuple-public-async-cancel-observer/results.json`). Una medición de
los conteos produjo valores fuera de rango que variaron entre procesos
(`tuple-public-async-cancel-counts/results.json`); no se debe asumir una causa ni
seguir usando esas posibles referencias inválidas como si fueran seguras. Dos
controles independientes transfieren solo el fuerte al generador, sin consultar
referencias potencialmente inválidas: con y sin await antes de `yield`, el payload
permanece vivo y se destruye una vez tras cancelar en Rc/Arc O0/O2
(`generator-moved-owner-probe/results.json` y
`generator-moved-owner-await-probe/results.json`). Queda por aislar la interacción
del observador débil, la captura y los contadores; este hallazgo no está corregido.
Después de esa observación, el LLVM identificó una desreferencia faltante al
tomar la dirección del parámetro capturado del generador. La corrección quedó en
`4f740a07d` con prueba independiente; los ocho cruces con observador y las cuatro
familias devolvieron 42 después del cambio. Los detalles, controles y límites
están en `async-generator-parameter-address.md`. Esto resuelve la reproducción
descrita, no acredita todas las posibles capturas o el SDK binario.

La ampliación con `SlowFail() throws Exception`, que suspende y luego lanza,
detectó inicialmente una construcción y ninguna destrucción antes del catch
(`tuple-partial-await-declared-throw/results.json`, salida 10 O0/O2). El ensayo
complementa las regiones temporales al propagar el error al catch y devuelve 42
en ambas configuraciones. También se detectó el mismo faltante al publicar el
error hacia un método llamador; el ensayo complementa esa ruta y las pruebas
`tuple-await-replay-propagate-after.log` y `tuple-await-propagate-finally.log`
pasan en O0/O2, incluido un finally que observa primero la destrucción. La forma
textual `true ? throw new Exception() : 7` no es aceptada por el parser; el
diagnóstico se conservó en `tuple-partial-regression-explicit-throw.log` y no se
ha tratado como una reproducción ejecutable. Las rutas con funclet de cleanup y
cancelación durante un await siguen sin acreditarse; el código sigue provisional.

El caso `await_in_finally` de `tuple-await-in-finally.log` pasa cuando el
finally se ejecuta tras una salida normal y el productor async lanza. En cambio,
si el finally comienza mientras ya se propaga una excepción y su `await`
suspende y termina normalmente, la excepción original no llega al catch. Los
contadores codificados como `100*caught + 10*builds + drops` son 11 con el
prototipo y 20 en la base, en O0/O2
(`tuple-unwind-finally-probe/results.json`). Sin suspensión en ese finally,
son 111 y 110 respectivamente (`tuple-unwind-finally-no-await/results.json`):
ambos capturan la excepción y solo el prototipo destruye el elemento. Esta
comparación apunta a un fallo previo en la propagación a través de suspensión
durante finally; no demuestra la causa raíz. La regresión
`await_in_unwind_finally` queda fallando y no se oculta ni se considera resuelta.
El LLVM posterior `tuple-unwind-finally-probe/llvm.log` muestra que el poll
publica el objeto de excepción y escribe estado `-2` antes de entrar al finally;
cuando el await suspende, escribe estado `1` en el mismo campo. Esto explica
por qué la reanudación no conserva por ese estado que había un error pendiente;
una corrección necesitaría preservar esa intención por separado hasta concluir
el finally. La evidencia no establece todavía la solución ni su manejo si el
finally también falla.
Una reproducción sin tuplas ni punteros (`async-finally-exception-probe/`)
termina con salida 1 en base y prototipo en O0/O2, cuando se espera 42 por la
captura. El MIR `async-finally-exception-probe/mir.log` marca el retorno original
con continuación excepcional, pero el `cleanup-continuation` del finally es
normal y el tramo siguiente publica resultado y estado de éxito. En el LLVM de
la reproducción con tupla, la reanudación entra en ese finally normal y termina
publicando `-1`. Por ello no basta restaurar `-2` después del await: también es
necesario conservar y reanudar la continuación correcta. No se ha aplicado
ninguna modificación específica a esa máquina de estados.
La regresión independiente `tests/integration/async_finally_continuation.py`
compara exception, exception con finally anidado, salida normal y return. En
`async-finally-continuation-before.log` y `async-finally-continuation-baseline.log`
ambas revisiones fallan los tres primeros tipos de continuación no normal
(exception, excepción anidada y return) en O0/O2, mientras la salida normal pasa.
El MIR de `async-finally-return-ir/mir.log` muestra que el valor 5 y estado -1
se escriben antes de entrar al finally; luego la continuación normal generada
puede publicar el valor predeterminado. Esto confirma que la corrección debe
conservar tanto el tipo de continuación como el resultado y respetar los
finally exteriores, no limitarse al caso de excepción. El tratamiento de una
segunda excepción continúa sujeto a la decisión ya consultada.
La ampliación con `break` que atraviesa el finally también falla en O0/O2 tanto
en base como en prototipo (`async-finally-continuation-break.log` y
`async-finally-continuation-break-baseline.log`): se pierde el destino del
bucle. El diseño debe conservar destinos de control, no solo el estado o valor
de un future. No se ha implementado esa conservación.

Una comparación dirigida posterior separa el flujo normal del retorno a través
de dos `finally` anidados con `await`. La salida normal ejecutó la secuencia
123 y terminó con 42 en O0/O2; el retorno conservó la secuencia de limpieza 12,
pero entregó 0 al llamador en lugar de 5 en ambos niveles. El fixture de
diagnóstico devuelve 20 para ese resultado y registra ambos fallos en
`A:/nsc-fixes-evidencia/async-finally-nested-diagnostic.log`; el ejemplo de
retorno anterior se mantiene sin cambios. Esto acota la reproducción al valor
de retorno en esa forma concreta, sin demostrar conservación correcta de
excepciones o destinos de bucle. El MIR de la variante simple ya conservada
escribe el valor del future antes del cleanup y contiene una salida normal
posterior que escribe el valor predeterminado; esa correlación no demuestra por
sí sola la secuencia exacta de todos los `finally` anidados. La prueba sigue
fallando y no se ha aplicado una corrección de esta continuación.

Un contraste adicional usa `await Immediate()` cuyo future termina en la
primera consulta. Retorno simple, retorno anidado, excepción y `break`
atravesaron su `finally` correctamente en O0/O2. Con el mismo esquema pero
`await Slow()`, que suspende, los cuatro fallaron en ambos niveles. Los
controles que retornan o lanzan *después* de un await ordinario también
terminaron con 42, al igual que el finally anidado en salida normal. Registro:
`A:/nsc-fixes-evidencia/async-finally-ready-control.log`. La selección incluye
ocho combinaciones fallidas y por ello termina con código 1; no es una prueba
aprobada. La comparación identifica la suspensión durante una continuación
de limpieza como condición de estos ejemplos, sin probar todas las rutas de
corrutinas ni resolver la semántica pendiente de una segunda excepción.

La selección dirigida se repitió con el compilador aislado de hash
`7c969cf811f592e3`. Los casos que suspenden dentro del finally siguen fallando
para excepción, retorno y break en O0/O2; los controles de salida normal y
await completado inmediatamente pasan en ambos niveles
(`A:/nsc-fixes-evidencia/async-finally-current-directed.log`). La selección
terminó con código 1 y no se considera una regresión aprobada. No se cambió el
backend de continuaciones en esta repetición.

La inspección MIR/LLVM se repitió sin sobrescribir la anterior mediante
`A:/nsc-fixes-evidencia/async-finally-return-ir-current.cmd`. En el LLVM actual
de `Work$poll`, el retorno guarda 5 y el estado terminal antes del cleanup. La
primera copia de `finally` suspende guardando el estado 1, pero el switch de
entrada asocia el estado 1 a `coro.resume.stmt.21`, la segunda copia de ese
finally. La continuación de esa segunda copia llega a `eh.try.cont.7`, que
escribe cero en el resultado. La asociación y las escrituras constan en
`async-finally-return-ir/llvm-current.log`. Esto explica el camino observado
para esa reproducción; no acredita la solución para excepciones ni para
finally anidados, donde el ensayo de identificadores únicos mencionado abajo
falló la verificación de dominancia de funclets.

Se ensayó asignar ids de suspensión distintos a cada copia de finally y volver a
publicar el estado terminal después de su limpieza. Los cinco casos iniciales
pasaron O0/O2 (`async-finally-state-restore-test.log`), así como `continue` y dos
await en un finally. Sin embargo, un retorno que atraviesa dos finally anidados
con await causó rechazo del verificador LLVM: una reanudación entraba en una
región cuyo cleanuppad no dominaba sus usos (`async-finally-extended-test.log`).
Limitar el cambio dentro de funclets no eliminó el rechazo. Se retiró ese ensayo
y se reconstruyó incrementalmente; el caso anidado vuelve a compilar pero falla
en ejecución como antes (`async-finally-revert-unsafe-test.log`). Por tanto los
resultados positivos del ensayo no son una corrección vigente. Se conserva la
regresión completa para una solución que maneje también el límite del funclet.
El 9 de noviembre se compilaron incrementalmente los objetivos unitarios
`nsc_ns3_t10_coroutine_tests`, `nsc_ns3_t10_async_function_tests` y
`nsc_ns3_t10_async_generator_tests`; sus tres entradas CTest pasaron mediante
`A:/nsc-fixes-evidencia/check-coroutine-targets.cmd`. Estas pruebas verifican
contratos unitarios y no demuestran la ejecución de la continuación suspendida
ni corrigen el fallo de `finally` descrito arriba.
Una prueba distinta hace que el productor suspenda y después falle por un
constructor estático. Tanto el ensayo como la base compilan en O0/O2, pero los
cuatro procesos terminan con código Windows 3763229441 sin llegar al catch:
`tuple-partial-await-throw/results.json` y
`tuple-partial-await-throw-baseline/results.json`. No se puede usar esa prueba
para acreditar la destrucción del elemento ni atribuir ese fallo al ensayo.

Con el compilador incremental actual se repitieron además seis casos existentes
de `async_finally_continuation.py` mediante
`A:/nsc-fixes-evidencia/async-finally-boundaries-current.cmd`. En O0/O2 pasaron
`continue` a través de un finally suspendido, dos finally anidados con salida
normal, y la sustitución de un retorno por una excepción lanzada después de
`await` tanto suspendido como listo. Fallaron el retorno a través de dos
finally suspendidos (salida 7) y la propagación de una excepción previa a un
finally con dos await (salida 8). Comandos y salidas están en el `.cmd` y el
`.log` homónimos; la selección termina con código 1. Estos controles distinguen
los casos observados, pero no prueban la liberación de propietarios ni el
tratamiento de una segunda excepción cuando ya había otra pendiente. No se ha
modificado la implementación de continuaciones ni se considera corregido el
defecto.

Control adicional de almacenamiento en arreglos: una estructura `Carrier`
contiene una tupla con `SharedView<Payload>` y se asigna a `Carrier[]` después
de crear la vista desde un propietario local. El análisis rechaza la asignación
para `Rc` y `Arc` con el diagnóstico de almacenamiento fuera de un local
rastreado (`A:/nsc-fixes-evidencia/view-tuple-carrier-array-probe.log`). La
regresión `view_tuple_struct_array_escape` conserva ese caso; con el dueño
vivo, `view_tuple_struct_live` devuelve 42 en O0/O2, y `plain_array_live`
devuelve 42 en O0/O2 para ambas familias. La selección dirigida pasó mediante
`A:/nsc-fixes-evidencia/view-tuple-carrier-array-regression.cmd`; su salida está
en el archivo `.log` homónimo. El compilador y SDK de esta prueba incluyen
cambios locales provisionales. No se ejecutó un acceso inválido ni se demuestra
que todas las formas de arreglos o agregados estén cubiertas.

Una estructura ordinaria con `SharedView` directamente en un campo se rechazó
en la declaración, incluso para el control local válido; por eso esa prueba no
acredita las rutas posteriores (`view-direct-carrier-probe.log`). Al declarar
la estructura como `borrowed`, los análisis rechazaron devolverla con un dueño
local, sobrescribir el dueño mientras vive la estructura y devolver un arreglo
que contiene la estructura. El control con dueño vivo se aceptó para `Rc` y
`Arc` (`view-borrowed-carrier-array-scope.log`). Se incorporaron estas cuatro
variantes a `shared_public_pointers.py`: las tres negativas se rechazaron y el
control válido ejecutó 42 en O0 y O2 para ambas familias
(`view-borrowed-carrier-regression.log`). Esta evidencia corresponde al árbol
provisional conjunto y no certifica otros tipos de contenedor ni consumo
binario de firmas con vistas compuestas.

La regresión binaria `shared_binary_pattern_field.py` se amplió con una
llamada mutante por alias, asignación de otro campo por alias y llamada sobre
otra instancia. Con el compilador aislado SHA-256
`dd8ef173cd74345f8d888052033ec37b7d66dde9ced36fd2cb332d70d0b3785c` y el
Core binario aislado preexistente SHA-256
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`, la
llamada mutante por alias se rechazó para Rc y Arc; los dos controles válidos
ejecutaron 42 en O0/O2 para ambas familias. La regresión completa dirigida pasó
(`A:/nsc-fixes-evidencia/shared-binary-pattern-field-current.log`); no se
ejecutaron los accesos rechazados. Esto no regenera el SDK ni acredita tuplas
binarias de vistas, DLL o todas las formas de alias. Cambió solo la prueba y
continúa sin commit.

La prueba binaria se amplió después a `outer.inner.value.Get()` para lectura,
reemplazo y movimiento del campo, reemplazo del receptor intermedio, llamada
mutante y escritura por alias. El control de lectura válido y los demás casos
se rechazan antes de analizar la protección esperada: el sujeto del patrón no
se reconoce como préstamo de un dueño identificador directo. La declaración
del alias de `outer.inner` también se rechaza. Los doce resultados Rc/Arc se
registraron como fallos de la expectativa en
`A:/nsc-fixes-evidencia/shared-binary-pattern-field-current.log`; ninguno de
los casos inseguros se ejecutó. Los controles directos anteriores siguen
pasando dentro de esa misma prueba. La implementación actual exige raíz
identificadora inmediata al crear el préstamo del patrón y al registrar campos
de su procedencia; habilitar la lectura anidada exige proteger tanto el campo
como cada receptor intermedio y sus alias antes de aceptar la referencia. No
se ha modificado esa implementación ni se presenta la regresión como aprobada.

Para aislar el alias del diagnóstico anterior, se sustituyó únicamente en el
fixture la declaración `borrowed Holder alias = outer.inner` por la referencia
local ordinaria `Holder alias = outer.inner`. Así ya no se diagnostica la
declaración, pero el patrón anidado sigue rechazado antes de verificar la
escritura mediante alias, en Rc y Arc
(`shared-binary-pattern-field-nested-borrowed-alias.log` conserva la versión
anterior; `shared-binary-pattern-field-current.log` conserva la nueva). La
regresión sigue teniendo doce fallos; no se ejecutó el caso inseguro ni se
concluye que el alias esté protegido una vez admitida la vista anidada.

El control válido que actualiza `outer.inner.other` durante el préstamo de
`outer.inner.value` también se agregó a la prueba. Se rechaza en la misma
barrera inicial para Rc y Arc; la selección actual contiene catorce fallos y
se conserva en `shared-binary-pattern-field-current.log`. Este control exige
que una futura solución distinga campos independientes, pero aún no ejecuta.

Para el ensayo se conservará la raíz de una cadena directa de campos sin
indexadores ni llamadas intermedias, cuando el `Get()` resuelto devuelve
referencia ligada a `this` de un campo move-only y la raíz es un local RAII.
El registro existente ya recopila los nombres de todos los campos y la raíz;
se aprovechará para rechazar también reemplazos de campos de referencia que
sean ancestros de la vista, no solo campos declarados propietarios. Esto es
conservador ante nombres repetidos y alias. La prueba debe rechazar cambios del
dueño, de `inner` y llamadas mutantes, y ejecutar lecturas y modificaciones del
campo independiente. La declaración `borrowed Holder alias = outer.inner`
permanece aparte porque tiene otra barrera; el control de alias ordinario sí
debe protegerse. No se habilitan indexadores, receptores calculados ni formas
de origen desconocido. La compilación será incremental y las pruebas dirigidas;
no se modifica interoperabilidad, el SDK instalado ni el formato de metadata.

El ensayo se implementó en el análisis del patrón y del reemplazo de campos;
el build incremental pasó (`nested-pattern-field-build.log`), con compilador
SHA-256 `2e2aed370a4c662bb5d8234360a7f76852e9107111b4a473e027dfeae8bbe9ff`.
La regresión binaria ampliada pasó para Rc y Arc: lectura anidada y cambio de
otro campo ejecutaron 42 en O0/O2; reemplazo, movimiento, alias, reemplazo del
receptor intermedio y llamada mutante se rechazaron con los diagnósticos
esperados (`shared-binary-pattern-field-current.log`). Los controles directos
de patrón, retorno prestado y operador local también pasaron
(`nested-pattern-field-pattern-control.log`, `nested-pattern-field-return-control.log`,
`nested-pattern-field-operator-control.log`). El Core binario es el artefacto
preexistente identificado arriba; no se regeneró. No se ejecutaron las variantes
rechazadas. La comparación por nombres puede rechazar campos distintos de igual
nombre y no acredita indexadores, alias prestado inicializado desde campo,
receptores calculados ni todas las cadenas; el ensayo sigue sin commit.

La regresión binaria se amplió a una cadena de tres campos y a reemplazar el
objeto intermedio después de terminar el patrón. Para Rc y Arc, la lectura
profunda y el reemplazo posterior ejecutaron 42 en O0/O2; el reemplazo de
cualquiera de los dos receptores intermedios durante el préstamo se rechazó
con el diagnóstico esperado. La prueba dirigida completa pasó
(`shared-binary-pattern-field-current.log`), con el mismo compilador y Core
binario preexistente indicados arriba. No se modificó el compilador en esta
ampliación y no se ejecutaron los casos rechazados. La prueba no extiende la
garantía a indexadores, receptores calculados ni todas las formas de alias.

Se añadieron seis casos equivalentes a `shared_public_pointers.py` para compilar
las familias desde las fuentes de esta copia. Rc y Arc ejecutaron la lectura
anidada y la actualización de otro campo en O0/O2; reemplazo, movimiento,
reemplazo del receptor intermedio y llamada mutante se rechazaron con sus
diagnósticos específicos. La selección CTest dirigida terminó correctamente
(`A:/nsc-fixes-evidencia/nested-pattern-field-source-controls.log`). No se
ejecutaron los casos rechazados ni se reconstruyó el SDK binario; estos
resultados se suman a la prueba de biblioteca preexistente, sin acreditar las
formas excluidas arriba. La selección no es la batería completa de punteros.

La prueba dirigida `shared_binary_field_constructor_failure.py` consume Core
binario preexistente y verifica campos `Rc`, `WeakRc`, `Arc` y `WeakArc` en un
objeto cuyo constructor falla, tanto después de inicializar el campo mediante
un método como dentro de ese método. Los recuentos fuertes y débiles esperados
se comprobaron en O0/O2: las 16 ejecuciones devolvieron 42
(`A:/nsc-fixes-evidencia/shared-binary-field-constructor-public.cmd` y `.log`).
El primer fixture utilizó un constructor implícito de `Payload`, no accesible
desde la plantilla binaria, y las 16 compilaciones se rechazaron antes de probar
cleanup (`shared-binary-field-constructor-current.log`); se ajustó el fixture
propio a un constructor público y se conservaron ambos logs.

Se utilizó el compilador aislado SHA-256
`46b70ba2a898622cb80cda26a94a826f21b5b2b620f5cb9c08e0cc9effd7ecdc` y
`pointers-sdk-refresh/NativeSharpCore.lib` preexistente, SHA-256
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`.
No se regeneró el SDK. El contenedor y su constructor están en la fuente del
consumidor; esto no verifica un contenedor precompilado, descarga de módulo,
otros campos ni liberación directa del bloque. La prueba y el ensayo de
cleanup permanecen sin commit y no certifican las cuatro APIs completas.

La prueba adicional `shared_binary_owner_constructor_failure.py` compila
`Payload` y `Holder` con su constructor en una biblioteca separada que consume
Core, y después compila el consumidor. Las 32 combinaciones de productor y
consumidor O0/O2 compilaron, pero fallaron en ejecución
(`A:/nsc-fixes-evidencia/shared-binary-owner-constructor-diagnostic.cmd` y
`.log`). La codificación del retorno muestra excepción capturada y un contador
de más: 32 en fuertes (2 en vez de 1) y 31 en débiles (1 en vez de 0), para
Rc y Arc y para fallo dentro del helper o después de su retorno. La misma
prueba con el constructor fuente en el consumidor pasó como se documentó
arriba. No demuestra por sí sola liberación física del bloque.

Inspección inicial: `sharedEmptyDefault` en `OopLlvmStatements.cpp` descarta
layouts importados antes de comprobar la forma y el destructor; esa condición
es una posible diferencia entre los escenarios, no una causa raíz demostrada.
No debe eliminarse sin acreditar que el estado vacío y su destrucción son
seguros para el tipo importado real y sin conservar el rechazo de homónimos.
No se ha modificado el backend. La regresión binaria permanece fallida, sin
commit, y debe diagnosticarse antes de cambiar la política de limpieza.

La inspección dirigida `shared-binary-owner-inspection/dump.cmd` descarta la
primera hipótesis para la reproducción `WeakRc`: el MIR muestra los layouts
de la envoltura y su almacenamiento como `imported=false`. El LLVM del
constructor solo registra cleanup del parámetro movido y no del campo; el
contador débil queda vivo. En ese mismo MIR no aparece una función de la
especialización exterior `WeakRc`, aunque sí sus layouts y destructores
internos. El criterio actual exige encontrar una función exterior para probar
la identidad de la familia. `HirBuildCoordinator.cpp` obtiene esa identidad
del texto de especialización de la declaración del tipo. Se ensayará consultar
la misma identidad de la declaración en el criterio existente, manteniendo
todas las verificaciones de estructura, destructor nulo y solapamiento; no se
tratará un nombre como prueba suficiente ni se alterará la regla de structs
ordinarios. Deben pasar la reproducción binaria y los controles de homónimos
y de inicialización de campos antes de considerar válida la corrección.

El ajuste en `OopLlvmStatements.cpp` consulta la identidad de especialización
en la declaración del tipo cuando no hay métodos exteriores emitidos; mantiene
la comprobación previa de namespace, layout y destructor protegido por null.
El build incremental pasó (`shared-binary-owner-identity-build.log`), con
compilador SHA-256
`1b7d83d0e089fbe073942a63a0035c3de63e382906692709221a865e24ade4cd`.
Las 32 combinaciones que dejaban un contador de más ejecutaron 42
(`shared-binary-owner-identity-after.log`). Los 16 controles de homónimos
directos y anidados ejecutaron 42 (`shared-binary-owner-identity-controls.log`).

La selección dirigida de construcción desde fuentes pasó
(`shared-binary-owner-identity-source.log`), al igual que los controles
existentes de consumo de Core binario (`shared-binary-owner-identity-binary.log`).
El Core consumido es el artefacto preexistente SHA-256
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`;
no se regeneró ni se atribuye al compilador actual. Esta evidencia cubre los
contadores y los casos listados, no demuestra liberación física del bloque,
DLL ni todos los layouts. El cambio es provisional y permanece sin commit.

La variante posterior `--dll` de `shared_binary_owner_constructor_failure.py`
compiló el constructor de `Holder` como DLL y pasó las 32 combinaciones de
Rc, WeakRc, Arc y WeakArc, fallo en helper o constructor y productor/consumidor
O0/O2. El control estático de la misma prueba también pasó sus 32 ejecuciones
tras la ampliación. Comandos y salidas en
`A:/nsc-fixes-evidencia/shared-binary-owner-constructor-{dll,static}-current.cmd`
y `.log`. Se utilizó el Core binario preexistente de la campaña y el árbol
del compilador con cambios locales. La DLL permaneció cargada; no se midió la
liberación física del bloque ni se verificó la descarga dinámica.

La selección `shared_field_method_borrow.py` se repitió con el compilador
incremental y el Core binario preexistente (`shared-field-method-current-directed.cmd`
y `.log`). En Rc y Arc rechazó las formas de reemplazo durante el préstamo
probadas por receptor, alias, argumento, delegado y evento; las lecturas y
reemplazos independientes o posteriores al ámbito ejecutaron 42 en O0/O2.
Esto actualiza los pasajes históricos que describen la llamada indirecta como
pendiente: esos pasajes reflejan el estado anterior, no una decisión aún por
pedir. La selección no acredita todas las rutas de alias o de código binario.

La regresión de `finally` asíncrono se repitió con el compilador SHA-256
`1b7d83d0e089fbe073942a63a0035c3de63e382906692709221a865e24ade4cd`
y el mismo Core binario preexistente. Los cuatro controles de salida normal
ejecutaron 11 como se esperaba. Las cuatro variantes con excepción y suspensión
sin clon de tupla ejecutaron 12 frente a 21: no se observa captura de la
excepción y queda un fuerte adicional. Las otras doce combinaciones con
excepción y clon no compilaron por `Cleanup funclets for the SEH personality
cannot contain exceptional actions`. Comando y log:
`A:/nsc-fixes-evidencia/shared-finally-binary-current.cmd` y `.log`. Se
confirman esos resultados, no una causa nueva ni una corrección. Core no se
reconstruyó y la prueba no acredita liberación física del bloque.

Para el compilador actual se generó además el LLVM del contraste `Arc`, sin
clon en la tupla, con Core binario y fuente de inspección conservada en
`A:/nsc-fixes-evidencia/shared-finally-binary-current-ir/`. En `Work$poll`, el
despacho de estado 1 apunta a `coro.resume.stmt.39`, la copia asociada a
`eh.finally.normal.8`; la salida de esa copia continúa a la salida normal del
try. La copia que procesa la salida pendiente contiene por separado la
destrucción del propietario capturado. El resultado confirma en el build actual
la divergencia de continuaciones observada en el LLVM anterior. No basta cambiar
un número de bloque: retornos, excepciones, finally anidados, cleanup y funclets
requieren conservar qué salida estaba pendiente antes de suspender. No se ha
modificado la implementación ni probado una solución de ese contrato.

La inspección del lowering identifica el punto donde convergen esas copias:
`collectCoroutineSuspensions` asigna un identificador al nodo MIR de suspensión,
y `enterCoroutineResumeTarget` guarda un único bloque por identificador en
`resumeBlocks`, sustituyendo la asociación anterior al emitir otra copia.
`lowerTryRegion` emite la limpieza normal por separado y `OopLlvmCleanup.cpp`
emite acciones de limpieza dentro de un `cleanuppad` para el desenrollado
excepcional. Esta lectura explica por qué distinguir copias es necesario, pero
no basta para garantizar dominancia de los funclets ni persistencia de la salida
pendiente. El siguiente cambio debe resolver ambas restricciones y repetir los
controles de retorno, excepción y finally anidados; todavía no hay corrección.

El LLVM actual aporta una restricción adicional para el diseño: la copia
excepcional comienza en `eh.cleanup.1.12`, con `eh.cleanuppad1`. Si su hijo
queda pendiente, `await.suspend.22` escribe el estado 1 y ejecuta `ret void`;
si está listo, `await.frame.kept.24` continúa mediante `cleanupret` del mismo
pad. Una segunda llamada a `Work$poll` es otra activación y no recupera el
desenrollado nativo abandonado por ese `ret`. Por ello asignar un id distinto
y apuntar de nuevo a un bloque dentro del pad no conserva por sí solo la
excepción ni el contexto del sistema de excepciones. La solución debe sacar
la continuación suspendible del desenrollado nativo y conservar explícitamente
la excepción y su propiedad; la forma concreta y las excepciones extranjeras
siguen sin demostrarse. Evidencia: líneas 468–549 del LLVM en
`shared-finally-binary-current-ir/llvm.log`. No se ejecutó una prueba nueva.

El contraste nuevo `exception_ready_no_clone` utiliza la misma excepción y
tupla de enteros del caso suspendido, pero `Slow` termina en su primera
consulta. Con el compilador y Core binario aislados usados en la prueba previa,
Rc y Arc devolvieron 21 en O0 y O2: la excepción se capturó y solo quedó el
fuerte original. El caso suspendido equivalente había devuelto 12. El comando
y resultado están en `A:/nsc-fixes-evidencia/shared-finally-ready-no-clone.cmd`
y `.log`. La prueba dirigida permite seleccionar el caso mediante
`NSC_SHARED_FINALLY_CASE`; sin esa variable continúa ejecutando todos sus
casos. Este contraste no prueba llamadas que lanzan dentro del funclet ni
resuelve la continuación suspendida.

El control `exception_catch_suspended_clone` sustituye el `finally` del
fixture por un `catch` que forma la misma tupla con `owner.Clone()`, espera a
`Slow()` y vuelve a lanzar la excepción. Las cuatro combinaciones de Rc/Arc y
O0/O2 devolvieron 21 con el mismo compilador y Core binario aislados; comando y
salida en `A:/nsc-fixes-evidencia/shared-catch-suspended-clone.cmd` y `.log`.
Esto demuestra el caso concreto de conservar la excepción y los propietarios
en un catch suspendido, sin la acción excepcional prohibida dentro del
cleanuppad. Es evidencia para investigar una captura seguida de limpieza en
código ordinario; no demuestra equivalencia de un finally general, ni cubre
excepciones extranjeras, salidas no excepcionales o finally anidados.

La inspección del frame limita la reutilización directa de ese mecanismo:
`appendCoroutineCatchOwners` reserva ocho bytes por nodo MIR `CatchRegion`, y
`beginCoroutineDispatch` asigna esos espacios a los mismos nodos; un
`FinallyRegion` sin catch no recibe uno. `emitOneTryCatchBody` guarda ahí el
objeto antes de bajar el cuerpo, y la salida gestionada libera o transfiere su
propiedad. En cambio, `lowerStructuredExit` guarda el valor de retorno y
recorre las limpiezas antes de elegir la continuación; el índice del bucle y
el recorrido son decisiones de generación de código, no estado persistente
del frame. Por tanto una transformación que capture la excepción debe
reservar almacenamiento para su región y conservar también el destino
estructurado; un único indicador compartido sin demostrar anidamiento no
acredita el contrato. Aún no se ha implementado esa transformación.

El selector `NSC_SHARED_FINALLY_CASE` ahora rechaza nombres desconocidos en
vez de terminar con éxito sin ejecutar casos. `shared-finally-unknown-case.cmd`
salió con código 1 y el diagnóstico esperado; el control válido de catch con
clon volvió a pasar las cuatro combinaciones en
`shared-catch-suspended-clone.log`.

El control `exception_catch_nested_finally_clone` conserva una excepción en
un catch, entra normalmente en un try anidado, clona el propietario y espera
en el finally de ese try antes de volver a lanzar la excepción exterior. Rc
y Arc devolvieron 21 en O0/O2; comando y salida en
`A:/nsc-fixes-evidencia/shared-catch-nested-finally-clone.cmd` y `.log`.
La prueba demuestra ese anidamiento con salida normal del try interior; no
demuestra finalmente anidados atravesados por un retorno o una excepción,
ni sustituye la corrección pendiente.

La regresión `child_error_finally_suspend` distingue un error devuelto por
un future hijo de un `throw` directo: `Fail()` se suspende y falla, después el
finally de `Work` espera a otro future que también se suspende. Con el
compilador aislado, la limpieza se completó (`Counter.Log == 1`), pero el
catch exterior no se ejecutó: salida 11 frente a 42 en O0 y O2. El comando y
la salida están en `A:/nsc-fixes-evidencia/async-finally-child-error.cmd` y
`.log`. El caso se compiló desde fuentes mínimas y no usa los punteros ni el
Core binario. La primera versión del fixture se rechazó porque faltaba una
sentencia de retorno para el camino normal; se añadió `return 0` para que el
contraste pueda ejecutarse. No se ha corregido la propagación de ese error.
El control `child_error_finally_ready` conserva el mismo future hijo fallido,
pero el await del finally termina inmediatamente: ejecutó 42 en O0/O2
(`async-finally-child-error-ready.cmd` y `.log`).

Tras el cambio de despacho de overrides sellados se repitió la regresión
dirigida `shared_binary_public_pointers.py` con el compilador incremental y
`pointers-sdk-refresh/NativeSharpCore.lib`. Terminó con código 0: incluyó
propietarios, factorías y limpieza de constructores de Rc y Arc en O0/O2,
restricciones de solo lectura y controles de invalidación de caché. Conserva
rechazos esperados para bases precompiladas; no acredita que esas bases estén
soportadas ni reconstruyó Core. Comando y salida:
`A:/nsc-fixes-evidencia/pointers-after-sealed-dispatch.cmd` y `.log`.

Para ese contraste se conservaron fuente, MIR y LLVM en
`A:/nsc-fixes-evidencia/async-finally-child-error-ir/`. En `Work$poll`, la
ruta de error del hijo guarda el objeto en el campo de error del future,
escribe estado -2 y entra a la copia `coro.resume.stmt.36` del finally. Si
`Slow` se suspende, esa copia escribe estado 2; el dispatch de la siguiente
llamada envía 2 a `coro.resume.stmt.49`, la copia de salida normal. Esta
termina el finally y continúa por la salida normal, no por el `ret` de la
copia de error. Por tanto, para esta reproducción concreta no basta volver a
marcar error: también hay que recuperar la continuación correcta y sus
cleanups. La copia de cleanup excepcional nativo es otra vía distinta. El
volcado es evidencia diagnóstica y no constituye una corrección.

La prueba binaria de patrones incluye ahora un elemento de arreglo:
`holders[0].value.Get()`. La lectura con el arreglo y el propietario vivos se
rechaza para Rc y Arc; los intentos de reemplazar el campo o el elemento se
rechazan en la misma barrera inicial, antes de la comprobación de invalidación
esperada. La selección termina con seis fallos
(`A:/nsc-fixes-evidencia/shared-pattern-indexed-current.cmd` y `.log`); no se
ejecutaron los casos inseguros. El análisis de la vinculación especial exige
actualmente que la cadena de campos termine en un identificador local RAII;
el indexador interrumpe esa cadena y un arreglo no satisface esa condición.
Aceptar la lectura exige registrar el arreglo y proteger tanto su elemento como
el campo, su vida y posibles alias. No se modificó el compilador ni se considera
corregido el caso; los diagnósticos específicos exigidos por la prueba son una
expectativa de la regresión, no evidencia de que la protección ya funcione.

Corrección de alcance de esa expectativa: el arreglo del fixture es plano y no
posee sus elementos de clase. Reemplazar su referencia no destruye el objeto
anterior, de modo que no se debe clasificar automáticamente como invalidación.
La prueba conserva el objeto anterior en un local, reemplaza el elemento y
espera leer la vista antes de liberar ambos objetos y el arreglo. Añade por
separado dos negativos que intentan destruir el objeto, directamente y por
alias, mientras la vista está viva. Estos últimos necesitan un diagnóstico de
vida verificable y no deben ejecutarse si se aceptan indebidamente.
La repetición de la selección actual registra diez fallos: cinco variantes
para cada familia, todas detenidas todavía en la barrera de creación de vista
(`shared-pattern-indexed-current.log`). No demuestra aún si los controles
positivos ejecutarán ni si los negativos se rechazarán por la causa correcta.

Se ensayó reconocer la raíz de un arreglo local y comprobar `delete` y
`destroy` mientras la vista vive. El ensayo pasó los controles directos de
lectura, sustitución plana, campo independiente y destrucción directa o por
alias; también pasó los controles anteriores de patrones y llamadas. Sin
embargo, `Holder alias = Identity(holders[0])` seguido de `delete alias` dentro
del patrón fue aceptado por el análisis para Rc y Arc. La prueba no ejecutó el
programa potencialmente inseguro y registra ambos fallos en
`A:/nsc-fixes-evidencia/shared-pattern-indexed-prototype-test.log`. La búsqueda
de raíces del ensayo no seguía ese resultado de función.

Por esa vía sin protección se retiraron los cambios del ensayo en el análisis
del patrón y de destrucción explícita, se reconstruyó `nsc` incrementalmente
y se repitió la regresión. La barrera inicial vuelve a rechazar la creación de
la vista, y las 26 expectativas indexadas continúan fallando antes de demostrar
la protección (`shared-pattern-indexed-current.log`). El código y los cambios
preexistentes se preservaron; el archivo de destrucción quedó sin diff. Los
controles dirigidos del ensayo no certifican una solución vigente. Falta seguir
de forma segura la procedencia a través de llamadas y conservar los casos
independientes antes de volver a habilitar esta forma.

Un segundo ensayo trató de forma conservadora los alias obtenidos por llamadas
o campos estáticos y comprobó tanto `delete` como `destroy`. Los controles de
esas formas llegaron al diagnóstico esperado, pero el ensayo aceptó destruir un
parámetro cuyo objeto se había guardado antes en `holders[0]`; la vista seguía
activa. No se ejecutó el programa. La salida está en
`A:/nsc-fixes-evidencia/shared-pattern-indexed-prototype-test.log`. El arreglo
no registra todavía las posibles procedencias de sus elementos: seguir solo
la raíz del arreglo o la procedencia del alias no relaciona ese parámetro con
el objeto prestado. También habrá que considerar escrituras mediante alias del
arreglo y conservar la independencia cuando realmente pueda demostrarse.

Se retiró el segundo ensayo y se reconstruyó incrementalmente `nsc`; la
repetición `shared-pattern-indexed-current.log` vuelve a rechazar el patrón en
su barrera inicial. Los nuevos controles estáticos se ejecutan solo con Arc:
un campo estático que contiene Rc está prohibido por decisión del usuario, y
el fixture anterior declaró ese campo para todos los casos por error. El
fixture se corrigió para no invalidar la selección completa; el log anterior
de esa tentativa no demuestra los controles de patrón. Este trabajo no
constituye una corrección del compilador y no se ejecutaron casos inseguros.

La regresión conserva además tres formas que la siguiente solución debe
distinguir: guardar el parámetro mediante un alias del arreglo, compartir un
objeto entre dos arreglos y llenar el arreglo mediante una función antes del
patrón. El compilador restaurado rechaza estas variantes en la misma barrera
de creación de la vista, para Rc y Arc
(`A:/nsc-fixes-evidencia/shared-pattern-indexed-current.log`). Por tanto no
se demuestra aún la protección de sus operaciones de destrucción. Ninguno de
los programas inseguros se ejecutó. El seguimiento de contenidos debe tener
en cuenta escrituras indirectas y llamadas; una tabla limitada únicamente a
asignaciones directas no bastaría para esos controles.

### Arreglos con limpieza exterior activa

Actualización de la selección indexada en `6b5b0fcb9`: con el compilador
incremental y `A:/nsc-fixes-evidencia/integracion-vistas-sdk/NativeSharpCore.lib`,
la selección de todos los casos `indexed_` de `shared_binary_pattern_field.py`
terminó correctamente. Realizó 79 comprobaciones semánticas y 36 emisiones
de controles positivos O0/O2, que ejecutaron con 42. Los casos que esperaban
rechazo no se ejecutaron; tres variantes Rc de almacenamiento estático se
omitieron conforme a la prohibición adoptada. El comando y la salida están en
`A:/nsc-fixes-evidencia/shared-indexed-selection-20261113.cmd` y `.log`.
Esto contradice el rechazo general observado en los ensayos históricos de la
sección anterior, pero no identifica qué cambio posterior lo eliminó ni
demuestra todas las procedencias, concurrencia o vida de módulos. El SDK
binario se había construido previamente en A.

Los controles de `destructor_throw_owned_field.py` reprodujeron una pérdida de
limpieza al lanzar el destructor de un elemento de un arreglo local `owned`
cuando había otro recurso activo: el arreglo de estructuras produjo 23 en vez
de la secuencia 213, y el arreglo de clases produjo 13 en vez de 123, tanto en
O0 como en O2 (`A:/nsc-fixes-evidencia/dynamic-active-before.log`).

En `OopLlvmCleanup.cpp` se permite usar la limpieza excepcional del arreglo
cuando hay una limpieza exterior activa, conservando su destino. El generador
de la función auxiliar para referencias guarda y despeja temporalmente las
regiones de limpieza de la función llamadora, para no insertar referencias a
variables de esa función en la auxiliar. La regresión posterior registra salida
42 para los dos controles en O0/O2 y salida 148 para la segunda excepción en
ambos tipos de arreglo (`destructor-release-registered.log`, 1/1). Los controles
de destructores de punteros compartidos pasaron
(`shared-destructor-throw-current.log`); las pruebas de destrucción de tuplas y
valores de SDK binario pasaron 2/2 (`struct-drop-regression.log`). La prueba
dirigida de ASan pasó 1/1 (`asan-throwing-array-registered.log`): demuestra la
liberación del bloque en los escenarios instrumentados existentes, sin afirmar
ausencia general de fugas. Compilador incremental aislado SHA-256:
`1225f90ae8fcc71472212f77b374f582781c8cddf3d0d0b685cf1201df74b0a1`.

Estos resultados no cubren la limpieza dentro de un funclet activo ni todas las
clases posibles de elementos de arreglo. La batería completa no se ejecutó,
conforme a la instrucción vigente; la campaña y sus cambios locales siguen
abiertos.
