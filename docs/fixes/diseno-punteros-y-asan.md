# Diseño autorizado: punteros compartidos y AddressSanitizer

Este documento fija la dirección de trabajo; ninguna de estas dos capacidades
se considera implementada ni validada por aparecer aquí.

## Rc, WeakRc, Arc y WeakArc

El contrato `NSSMARTPTR` publicado por el compilador exige clonación explícita,
ausencia de copia implícita, upgrade sin resurrección, rechazo de overflow del
contador y mantenimiento de las funciones de destrucción y liberación del
módulo productor mientras exista el bloque de control. `Rc` y `WeakRc` son
locales; `Arc` y `WeakArc` pueden cruzar hilos solo cuando el payload satisface
Send/Sync. Las familias y fábricas pertenecen a `NativeSharp.Core`, que es el
namespace usado por la reproducción, y deben respetar el contrato publicado.

Cada bloque conserva contadores fuertes y débiles, origen del allocator,
destructor, liberador y procedencia del módulo. El último fuerte destruye el
payload una vez y libera la referencia débil implícita; el bloque solo se
libera al desaparecer el último débil. Upgrade incrementa el fuerte solo si
es distinto de cero, mediante compare-exchange en Arc. Las operaciones que
no puedan incrementar deben fallar antes de desbordar. Clone y Downgrade son
explícitos; un valor expirado nunca vuelve a poseer el payload.

La implementación debe acreditar además la fábrica de una sola asignación
exigida por el manifiesto, el movimiento sin doble liberación, el pin del módulo
y la imposibilidad de copiar o compartir Rc de forma indebida. El SDK contiene
`ReferenceCounts` atómico y hooks en `NativeSharp.Core.Threading`, pero no
provee los cuatro tipos ni demuestra las demás garantías. No basta añadir
declaraciones vacías ni tratar esos contadores como una implementación completa.
Es necesario investigar y probar el soporte efectivo del compilador para
propiedad genérica, destrucción, restricciones Send/Sync y módulo productor.
`NativeSharp.Core.Threading.nsproj` ya referencia `NativeSharp.Core`, mientras
`Interlocked` y `ReferenceCounts` viven en Threading. Por ello Core no puede
simplemente agregar una referencia inversa para implementar Arc: primero hay
que ubicar la primitiva compartida sin crear un ciclo, manteniendo disponibles
los nombres esperados por los consumidores de Core.
La búsqueda de `validateSmartPointerOperation` solo encuentra su definición
en `src/Language/SmartPointerSemantics.cpp`; no se ha encontrado una llamada
desde el frontend. El manifiesto de reglas, por tanto, no demuestra que hoy se
rechacen copias implícitas o cruces de hilo indebidos en programas reales.

En `A:/nsc-fixes-evidencia/smart-pointer-probe/` un prototipo descartable de
struct genérico con destructor poseyó un payload y ejecutó con salida 42; una
copia implícita fue rechazada (`smart-pointer-copy-probe/check.log`). Un
segundo prototipo local pasó clonación, expiración débil y destrucción única
en O0 (`smart-pointer-lifecycle/`). Sin embargo, un `Get()` que devuelve `T`
permite conservar una referencia después del último propietario
(`smart-pointer-escape/check.log`); cambiarlo a `borrowed T` se rechaza porque
el frontend no acredita la vida del campo (`smart-pointer-borrow/direct-check.log`).
Crear antes un préstamo local también se rechaza (`local-check.log`). Los
intentos de retornar `ref[this]` al campo también se rechazan con vida
`unknown` (`ref-this-check.log`). Los scripts en la carpeta de evidencia
reconstruyen cada variante por separado.
Una vista `borrowed struct` que contiene la referencia sí pasó el análisis,
pero también lo pasó al asignarla fuera del ámbito del último `Rc` y usarla
después (`smart-pointer-view-probe/escape-check.log`); esa comprobación no
demuestra un préstamo seguro del payload compartido.
El prototipo recibe además un objeto ya asignado y crea el bloque de control
por separado; no acredita la fábrica de una sola asignación del manifiesto y
solo ensaya payloads de clase.
Estos prototipos no son una implementación pública: no se han probado
overflow, upgrade, Arc, sincronización, pin del módulo ni O2, y la API de
acceso debe resolver explícitamente la vida del payload antes de publicarse.

## AddressSanitizer

El usuario aprobó exposición explícita y desactivada por defecto. La opción
debe seleccionar instrumentación de las unidades LLVM y enlazar la DLL y las
bibliotecas de runtime compatibles suministradas externamente; su ubicación
debe ser configurable y validarse sin alterar `C:/native-sharp` ni Clang.
El entorno contiene Clang 23 y los artefactos Windows `clang_rt.asan_dynamic`
en `C:/clang-llvm/lib/clang/23/lib/windows`, únicamente como referencia.

Prueba exploratoria posterior: se volcó IR de un programa NativeSharp con
escritura fuera de los límites de un `stackalloc`, se añadió el atributo de
sanitización a una copia del IR, y se aplicó `opt -passes=asan`. Clang enlazó
el objeto, el runtime NativeSharp de A, Shell32 y el runtime externo de ASan;
el ejecutable terminó con código 1 y ASan informó el acceso fuera de límites
(`A:/nsc-fixes-evidencia/asan-probe/run.log`, `link-result.json`). Esta prueba
usa `opt` y Clang directamente; no demuestra integración en `nsc`, NSLink,
productos particionados ni una opción pública funcional.
Un control con acceso válido pasó por el mismo recorrido y terminó con 42
(`A:/nsc-fixes-evidencia/asan-safe/results.json`).

Una segunda prueba mínima intentó enlazar el objeto instrumentado con NSLink
y las bibliotecas ASan y builtins de Clang. El enlazador de referencia rechazó
la variable externa `__asan_shadow_memory_dynamic_address` (NSL1020;
`asan-probe/nslink-dynamic-only-1.log`). Al añadir el thunk estático de Clang y la
importación de excepciones de Windows, ese símbolo dejó de ser el primer
error, pero apareció `__asan_default_options` sin resolver
(`asan-probe/nslink-thunk-import-1.log`). La prueba modifica una copia del IR para llamar
al inicializador y proporcionar un trap mínimo; por eso acota una dificultad
del enlace, pero no prueba que todas las rutas posibles de NSLink fallen.
`src/Linker/LldLinker.cpp` exige actualmente que `NSC_LINKER` sea nslink.exe,
de modo que usar LLD como alternativa explícita para ASan requiere trabajo de
integración y pruebas, no solo cambiar una variable de entorno.

`LlvmNativeSanitizer` ya inserta el atributo y aplica el pipeline `asan`; la
prueba existente verifica IR. Falta conectar el indicador desde el driver a
cada unidad generada, garantizar que las claves o la desactivación de caché
impidan reutilizar objetos sin instrumentar, resolver la interacción con
ThinLTO y las bibliotecas precompiladas, y enlazar y localizar el runtime al
ejecutar. Las combinaciones aún no soportadas deben rechazarse de forma clara,
no producir un binario aparentemente instrumentado.

Prueba adicional del arranque (`A:/nsc-fixes-evidencia/asan-entry-probe/results.json`):
con el mismo objeto instrumentado y la DLL de ASan en `PATH`, el control con
arranque convencional de Clang enlazó y devolvió 42. Al forzar `/entry:main`,
LLD informó símbolos CRT sin resolver; al agregar explícitamente `libcmt`,
`libvcruntime` y `libucrt`, enlazó pero el proceso terminó con `0xC0000005`.
Esto evidencia que cambiar la entrada o añadir bibliotecas no basta en esta
configuración experimental; no identifica por sí solo la causa de la violación
de acceso ni demuestra que no exista una integración posible. El arranque real
de NativeSharp utiliza `__nativesharp_start` y `/nodefaultlib`.

La validación mínima debe incluir una ejecución válida, una escritura fuera
de límites que ASan diagnostique realmente, O0/O2, ausencia de instrumentación
sin la opción, falta de runtime con diagnóstico, y aislamiento de cachés y
resultados. Aún no se ha demostrado el enlace ni la ejecución negativa desde
la opción de `nsc` y el enlazador NativeSharp.
