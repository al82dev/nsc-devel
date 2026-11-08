# Diseño autorizado: punteros compartidos y AddressSanitizer

Este documento conserva el diseño y la evidencia. Los punteros compartidos
siguen pendientes; ASan cuenta con la ruta limitada descrita más abajo.

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
Como primer paso, `SharedReferenceCounts.ns` añade a Core un estado interno
de contadores con cargas y compare-exchange secuencialmente consistentes. El
frontend reconoce exclusivamente las firmas internas de 64 bits para emitir
los mismos helpers atómicos usados por Threading. La prueba dirigida
`nsc.sdk.atomic-shared-counts` comprueba cero, último fuerte y débil, rechazo
de overflow y 200 000 ciclos distribuidos en cuatro hilos en O0/O2; el análisis
del proyecto Core completo también pasó (`shared-counts-test.log` y
`shared-core-check.log` en `A:/nsc-fixes-evidencia`). Es una primitiva interna,
no demuestra todavía las familias públicas, la destrucción del payload ni
seguridad de concurrencia para todos los intercalados posibles.
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
El compilador ahora rechaza esa asignación y otras formas de escape de la vista
ligada al receptor; véase `view-receiver-lifetime.md`. La referencia extraída
con `view.Get()` todavía puede sobrevivir al propietario, de modo que esto no
resuelve por completo el préstamo del payload.
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

`LlvmNativeSanitizer` inserta el atributo y aplica el pipeline `asan`; la
prueba existente verifica IR. El generador de objetos
dispone de una configuración interna que instrumenta cada unidad antes de la
optimización, desactiva su caché de objetos y rechaza LTO. La prueba
`nsc.project-oop-address-sanitizer` verifica en O0/O2 los símbolos de cada
objeto, los objetos normales antes y después, la reutilización de la caché
normal y el rechazo de ThinLTO (`A:/nsc-fixes-evidencia/asan-unit-test.log`).
El driver conecta esta ruta a `--emit-project-oop-exe` mediante
`--sanitize-address --asan-clang=<clang.exe> --asan-runtime=<DLL>`. La DLL
debe llamarse `clang_rt.asan_dynamic-x86_64.dll` y se publica junto al
ejecutable; Clang y la DLL deben ser versiones compatibles, responsabilidad
de la configuración suministrada. Se rechazan LTO, PDB, enlace incremental,
subsystem de ventana y caché de artefactos explícita; el modo ASan desactiva
la caché predeterminada. Bibliotecas precompiladas y runtime NativeSharp no se
instrumentan, por lo que los accesos dentro de esas bibliotecas no están
cubiertos. No se ha validado ASan para DLL, bibliotecas estáticas o ThinLTO.

La posición de esa pasada se comprobó con el mismo ejemplo de escritura fuera
de límites: `default<O2>` antes de ASan produjo una terminación sin diagnóstico
ASan (`asan-pass-order/results.json`); ejecutar ASan antes de `default<O2>`
conservó el diagnóstico `dynamic-stack-buffer-overflow`
(`asan-pass-before-opt/results.json`). Ambas pruebas usan IR y Clang externos,
no una opción de `nsc`; la prueba del generador comprueba además que el símbolo
de reporte persiste en el objeto generado en O0 y O2.

La prueba `asan-real-entry/results.json` avanzó a los objetos reales del
generador y a un módulo de entrada producido por `WindowsRuntimeBuilder`.
Un adaptador `main` generado por `WindowsRuntimeBuilder::buildSanitizerEntryAdapter`
llamó a `__nativesharp_start` después del arranque CRT, y
`WindowsExecutableBuilder` creó el adaptador y el objeto de entrada, reunió las
bibliotecas, y el método interno `LldLinker::linkAddressSanitizedExecutable`
invocó Clang/LLD para enlazar el runtime externo. El caso inválido produjo
`dynamic-stack-buffer-overflow` en O0 y O2; el control válido devolvió 42 en
ambos niveles. El fixture invocó el constructor fuera del driver.

La prueba posterior `nsc.driver-address-sanitizer` usa la opción real de
`nsc`, verifica DLL publicada sin añadirla a PATH, caso válido y caso inválido
en O0/O2, ejecución normal sin dependencia ASan y rechazo de varias
combinaciones incompatibles. Pasó junto a la prueba del generador: 2/2
(`A:/nsc-fixes-evidencia/asan-regressions.log`). Requiere configurar
`NSC_TEST_ASAN_CLANG` y `NSC_TEST_ASAN_RUNTIME`; sin ellas CTest la marca
omitida, no aprobada. Esta evidencia usa Windows x64, Clang 23 y el SDK aislado.

Prueba adicional del arranque (`A:/nsc-fixes-evidencia/asan-entry-probe/results.json`):
con el mismo objeto instrumentado y la DLL de ASan en `PATH`, el control con
arranque convencional de Clang enlazó y devolvió 42. Al forzar `/entry:main`,
LLD informó símbolos CRT sin resolver; al agregar explícitamente `libcmt`,
`libvcruntime` y `libucrt`, enlazó pero el proceso terminó con `0xC0000005`.
Esto evidencia que cambiar la entrada o añadir bibliotecas no basta en esta
configuración experimental; no identifica por sí solo la causa de la violación
de acceso ni demuestra que no exista una integración posible. El arranque real
de NativeSharp utiliza `__nativesharp_start` y `/nodefaultlib`.

Una prueba adicional de la vista prestada (`smart-pointer-view-asan-confirm/results.json`)
confirmó que el destructor del payload se ejecutó y después leyó el campo de
ese payload; el proceso devolvió 42 sin diagnóstico ASan. En esta configuración
el runtime de NativeSharp usa su propia ruta de asignación y no se ha acreditado
que ASan observe la liberación de esos objetos. Por tanto, este modo **no
garantiza detectar use-after-free en el heap administrado por NativeSharp**;
la prueba de stackalloc no demuestra cobertura de ese heap.

Las rutas no cubiertas y la compatibilidad entre versiones del runtime deben
permanecer visibles; una ejecución correcta no demuestra detección completa de
todo error de memoria.
