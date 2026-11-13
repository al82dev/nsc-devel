# Campaña de limitaciones del compilador y SDK

Base: `152654e5a0e2d84d4c2f245ea7590488f0497430`, rama
`development-fixes-limitaciones`. Fuentes: reporte y registro detallado de
`A:/native-sharp-reference`. Compilador y runtime compilados desde la copia en
`A:/nsc-fixes-evidencia/build`; SDK de la copia en `A:/nsc-fixes-evidencia/sdk`.
Windows x64, LLVM/Clang 23.1.0, host estático y Release. Se usa el enlazador
externo `C:/native-sharp/bin/nslink.exe` como dependencia de referencia y el
runtime generado en A. Un ejecutable de una etapa anterior informa NativeSharp
3.0.0 y build hash `c237186aba0246a4` (`final-nsc-version.log`); no identifica
las compilaciones incrementales posteriores. No se instala ni modifica
`C:/native-sharp`.

Las reproducciones son copias en `A:/nsc-fixes-evidencia/cases`; las fuentes
originales se conservan. Las copias de proyectos que usaban el SDK instalado
apuntan a las bibliotecas de la copia. Cada `result.json` registra el comando y
resultado inicial; `build.log`, `run.log` y demás salidas del directorio son
evidencia. Los resultados iniciales corresponden al compilador construido antes
de los cambios, no a la última ejecución. Las pruebas posteriores y sus límites
se detallan en los documentos de `docs/fixes` y registros de la misma carpeta.

| Caso | Resultado inicial y situación |
| --- | --- |
| Rc / WeakRc; Arc / WeakArc | `sdk-01`: nombres no encontrados en la base. La reproducción original y controles con clases y structs compilan y ejecutan con Core binario aislado. `0f25b9021` registra las condiciones de Send/Sync de Arc y WeakArc y sus pruebas dirigidas; la invalidación de caché se comprueba en `arc-conditional-cache.md`. Con Core regenerado desde esta copia, `pointers-sdk-refresh-test.cmd` pasó la prueba dirigida de propietarios, débiles, cleanup, factorías, solo lectura y cambios de productor/base; véase `A:/nsc-fixes-evidencia/pointers-sdk-refresh-scope.md`. La generación intentó reutilizar caché pero informó 0 hits. La vida del módulo, las vistas compuestas en binarios y otros recorridos siguen pendientes; los resultados usan un árbol con cambios locales y no acreditan el contrato completo. |
| Dictionary string AddOrUpdate | `sdk-02`: termina 3221226356; doble liberación. Corregido en `5127e4d07`, con ejecución de regresión. |
| SortWith | `sdk-03`: nombre no encontrado. Por decisión del usuario, `SortWith<T>(Span<T>, Comparison<T>)` delega en la sobrecarga existente de `Sort`. La reproducción original compiló y ejecutó con salida 0 en O0/O2 (`sortwith-original/results.json`); `nsc.sdk.sort-with-alias` verifica orden descendente, duplicados, vacío y un elemento, con ejecución 42 en O0/O2 (`sortwith-test.log`). El span vacío requiere tipo explícito en esta prueba. |
| Saturación: suma y conversiones a short y byte | `comp-01`: `-2147483648`, `-25536`, `34`; corregido para esas operaciones en `a7436961d` y `bb4c18900`. No se afirma cobertura de todas las operaciones saturadas. |
| Literales y cálculo decimal | `comp-02`: factura 101, comparación falsa, redondeo 100/100/101. `a63333883` conserva el lexema de literales y corrige la reproducción; no demuestra precisión completa de decimal ni inicializadores estáticos. |
| checked i128 | `comp-03`: la suma desbordaba y continuaba con wrapping. El usuario eligió trap; `93d859e31` corrige la suma firmada i128 con regresiones O0/O2. Otras operaciones no quedan cubiertas. |
| División i128 | `comp-04`: enlace falla por `__divti3`. `2f1ae8d47` corrige división firmada; pruebas diferenciales, extremos y divisor cero. Resto y unsigned no están cubiertos. |
| sealed O2 | `43-dispatch`: la base conservaba llamada indirecta. El árbol actual emite llamada directa cuando el método seleccionado es un override sellado y dispone de destino; conserva despacho virtual para métodos abiertos. La reproducción original se comprobó en MIR y las dos regresiones dirigidas de fuentes y biblioteca estática pasaron. Límites y evidencia en `sealed-override-dispatch.md`; el cambio se validó con otros cambios locales presentes. |
| Cancelación async | `comp-06`: traza 0 y salida 5. Con un avance adicional de `MoveNextAsync`, `comp-06-deferred` da traza 7 y salida 0. El usuario confirmó limpieza en el siguiente avance, conforme al contrato existente. |
| impl externo para struct | `68-struct` se rechazaba. El usuario autorizó despacho estático sin boxing; `f3b0314c0` lo implementa y añade regresión. |
| Delegado genérico con retorno T | `69-delegate`: rechazo de T. `3c03a4440` corrige el patrón reportado; otras formas de retorno genérico no están demostradas. |
| Restricción entre parámetros genéricos | `69-constraint`: la base rechazaba T. `6022fe794` implementó las restricciones dependientes; la reproducción original volvió a compilar y ejecutar en el árbol actual. La regresión dirigida comprueba relaciones válidas e incompatibles, llamadas al trait y consumo binario; véase `dependent-trait-dispatch.md`. La validación usó un árbol con otros cambios locales y no certifica una revisión limpia. |
| Lambda sin captura en Main global | `69-lambda`: falla en HIR por ordinal de owner. `f8a05890f` corrige función global sin captura; cierres con captura no están cubiertos. |
| Matriz dinámica genérica decimal | `70-decimal`: falta layout del elemento. `de6415700` añade el layout en operaciones de arreglo; reproducción y regresión pasan. |
| Método genérico en matriz genérica | `70-method`: la base lo rechazaba. El ejemplo original intacto pasa el análisis, compila y devuelve 6 en O0/O2; su `Main` no llama a `Multiply`. La selección dirigida de inferencia y consumo binario pasó 6/6. El ensayo sigue sin commit y no cubre todas las formas de receptor; evidencia y límites en `generic-owner-method.md`. |
| Función genérica con dimensiones const reenviadas | `70-static`: rechazaba Rows. `690a8ffd3` corrige el caso y verifica multiplicación y argumentos inválidos. No cubre todos los ámbitos o paquetes. |
| Temporal como argumento in de operador | `70-temporary`: la base rechazaba el argumento. En el árbol provisional actual, el ejemplo original ejecuta 0 en O0/O2 y la regresión de vida termina correctamente; siguen abiertos el retorno prestado de clase y el consumo de operadores binarios con carga `owned`. Evidencia y límites en `temporary-in-operator.md`. |
| Llamada a función desde #run | `73-run`: la integración `fc7ea5bb7` admite llamadas puras acotadas de la misma fuente. Cuatro controles dirigidos de argumentos, anidamiento, wrapping y división por cero pasaron en O0/O2 con el compilador aislado (`A:/nsc-fixes-evidencia/73-run-current-probe.log`). No es soporte general de llamadas ni comparación con el ejemplo original omitido por autorización; contrato y límites en `73-run-laptop.md` e `integracion-run73.md`. |
| Inspección general de campos | `74-fields`: el ejemplo original usa campos explícitos. La rama integrada también admite la iteración estática `Fields(T)` en los contextos acotados descritos en `static-field-query-phase-design.md`. Las pruebas dirigidas de fuente, binarios y certificados volvieron a pasar tras la integración; evidencia y límites en `campos-construccion-integracion-20261112.md`. No se afirma soporte universal para todas las jerarquías o rutas de alias. La observación de ausencia de superficie en `static-field-introspection.md` corresponde a una revisión anterior. |
| derive Print | `75-derive`: se conserva la salida existente `GridPoint`, que el propio ejemplo compara exactamente; incluir campos cambiaría ese contrato. El ejemplo original sin cambios compiló y terminó con 0 en O0/O2, con `Text=GridPoint` y las comprobaciones de igualdad y hash aprobadas (`A:/nsc-fixes-evidencia/75-print-contract-*-run.log`). No se afirma que exista un formateador de campos; la corrección acotada de igualdad está en `derive-ignore-equality.md`. |
| SIMD máscara a vector, indexación y condicional | Los rechazos de `89-mask-convert`, `89-mask-index` y `89-mask-select` se corrigieron en `e7eac1eb9`, `1769a179d` y `fbb5d736b`. Las tres regresiones y el control flotante a entero sin signo volvieron a pasar con el compilador aislado (`A:/nsc-fixes-evidencia/89-simd-current-*.log`). Los ejemplos originales pasaron O0/O2 en la integración previa; contratos y límites en `integracion-conversion-mascara.md`, `integracion-indice-mascara.md` e `integracion-seleccion-mascara.md`. |
| AddressSanitizer | No se localizó opción en la base. La rama incorpora `--sanitize-address` para ejecutables de proyecto de consola con Clang y DLL externos indicados explícitamente; la ruta utiliza Clang/LLD para el enlace y conserva NSLink para las bibliotecas de importación. Caso válido y desbordamiento pasan en O0/O2; restricciones y cobertura en `diseno-punteros-y-asan.md`. |
| Operadores ++ / -- | `39-increment`, `39-decrement`: la base rechazaba la declaración. El prototipo admite usos acotados sobre structs copiables y sobre un local RAII directo de clase con operador fuente o de biblioteca NativeSharp y retorno `owned`; hay pruebas dirigidas O0/O2 de propiedad, descarte y excepción desde fuente, con límites de cobertura. Las demás formas de destino de clase siguen rechazadas; el trabajo está sin commit. Evidencia y límites en `incremento-usuario-investigacion.md`. |
| NativeSharp.Text | `text-namespace`: namespace no encontrado. El usuario decidió mantenerlo pendiente hasta definir su superficie. |
| Compile con .. | `102-outside`: el rechazo de la base se corrigió con la integración `4e034d0a9`, que admite rutas relativas externas explícitas y globs sin ampliar la búsqueda implícita. La regresión dirigida pasó con archivo y glob en O0/O2; también comprobó exclusiones, errores y límites de enlaces (`A:/nsc-fixes-evidencia/102-verify-current.log`). El archivo fuente del ejemplo histórico falta, por lo que no se afirma su ejecución; véase `integracion-rutas-externas.md`. |
| Ejemplo 66 | Antecedente ya resuelto en el ejemplo; no se atribuye un defecto vigente al compilador. |

`96c68b2be` permite consultar `Expired()` y `WeakCount()` de referencias
débiles recibidas mediante `in`. Las regresiones dirigidas desde fuentes y
con una biblioteca mínima pasaron para ambas familias en O0/O2, incluidos
los estados vacío, vivo y expirado. La biblioteca mínima no es el SDK
completo; evidencia y límites en `weak-readonly-observation.md`. Los demás
recorridos de las cuatro familias siguen pendientes.

El ejemplo original 97 inicialmente devolvía 0 sin diagnóstico en O0/O2 pese
a su escritura fuera de límites. Con la integración provisional del adaptador
de heap ahora ASan informa `heap-buffer-overflow` en ambos niveles. También
pasaron los controles dirigidos de acceso válido y uso después de liberar en
ambos niveles. La prueba y los comandos están en
`A:/nsc-fixes-evidencia/asan-original-97-20261111/integrate-tests.log`; el
ejemplo se conserva sin cambios. La compatibilidad con runtimes personalizados
y otras configuraciones permanece sin verificar, por lo que ASan no se declara
terminado; los detalles están en `diseno-punteros-y-asan.md`.
La comprobación dirigida posterior pasó 4/4: incluye la ruta con runtime
integrado en O0/O2 y el rechazo de una copia de runtime sin la referencia de
reasignación esperada. Esto no verifica runtimes personalizados en general.
Un runtime alternativo reprodujo un falso negativo pese a conservar esas
referencias; la ruta ASan ahora exige que la biblioteca independiente coincida
con el runtime generado por el compilador y rechaza explícitamente los demás.
La regresión dirigida volvió a pasar; evidencia y coste en el documento ASan.
Se corrigió además el valor predeterminado ante reservas ASan imposibles: la
prueba dirigida verifica nulo y conserva el bloque previo tras una
reasignación fallida. El usuario puede sobrescribir esa opción del runtime.
Tras actualizar incrementalmente el auxiliar ASan, las cuatro pruebas
dirigidas volvieron a pasar con el compilador aislado actual, sin omisiones:
incluyen diagnóstico de desbordamiento y uso posterior a liberación en O0/O2.
El log y los hashes están en `diseno-punteros-y-asan.md`; siguen vigentes sus
límites de instrumentación y no se probó detección de fugas.

Durante la preparación del consumo binario de las familias se reprodujo una
limitación adicional de accesibilidad: un consumidor con `ProjectReference`
podía invocar un método público de una clase `internal` de otro proyecto,
incluso sin importar su namespace. La declaración local del mismo tipo se
rechazaba. `2aca52fb1` corrigió la vía alternativa de receptor estático para
esa forma y pasaron sus regresiones de fuentes; véase
`visibilidad-tipo-interno-estatico.md`. Aún no hay evidencia de ese acceso con
el binario del helper interno porque su generación falla antes.

Un ajuste provisional conserva los propietarios internos de métodos genéricos
en metadata. Una biblioteca mínima y su consumidor pasan O0/O2 y se conserva
el rechazo del acceso externo, pero Core continúa fallando en otra validación
de plantillas. El archivo tocado es compartido con infraestructura de
interoperabilidad; alcance y evidencia en `visibilidad-tipo-interno-estatico.md`.

La comprobación posterior de `Get()` reprodujo mutación y escape de arreglos
alcanzados por un alias de clase de solo lectura. `764137d50` conserva el
préstamo a través del arreglo; `6271b2d83` corrige la pérdida adicional al
atravesar un elemento struct que contiene una clase. La regresión dirigida y
sus controles pasaron para los controles local y atómico; el segundo cambio
restringe conservadoramente algunas copias por valor. Evidencia y límites en
`readonly-array-borrow.md`. Las familias públicas y los demás pendientes no se
consideran terminados por estas correcciones.
`5ac98b1b9` conserva además el préstamo al extraer una clase de una carga
struct devuelta por referencia de solo lectura; mantiene copias de estructuras
con campos escalares conocidos y conserva un tratamiento prudente para tipos no
demostrados. Sus regresiones y límites están en el mismo documento.
`058e2b622` permite la iteración por valor de arreglos prestados con origen
local rastreado y conserva las restricciones del préstamo; `4e9d0b4da` cubre la
lectura mediante `foreach ref` para ese mismo origen. Siguen pendientes las
fuentes sin procedencia rastreada. `d41121bbe` corrige el fallo de generación
con `break` que también se reprodujo con arreglos ordinarios; pruebas y alcance
en ese documento.
`79ebe0d7e` conserva el dueño al iterar directamente un campo de arreglo de
`owner.Get()` con vida `this`; pasó controles dirigidos por valor y referencia
para los controles internos y las API públicas desde fuentes. No acredita la
procedencia de temporales ni de todas las formas de receptor.
`e38863ba3` extiende ese seguimiento a `owner.Borrow().Get()` cuando la vista
conserva una raíz local comprobada; las fuentes con dueño temporal se rechazan.
`892d4cc81` añade una prueba de liberación del último débil explícito mientras
se ejecuta el destructor del último fuerte: pasó en O0/O2 y en su CTest dirigido.
Es evidencia del control interno, no de la transferencia pública entre hilos;
sus límites están en `shared-atomic-last-weak.md`.
`fd25c9df8` añade controles dirigidos de operaciones compuestas y ramas mezcladas
de arreglos prestados; ya se rechazaban y no se modificó el compilador. La
evidencia está en `readonly-array-borrow.md`.
`b106c3f6e` libera la reserva de las fábricas si falla la inicialización
estática de la carga. `023a7bc0a` limpia también campos propietarios cuando una
excepción implícita interrumpe la construcción placement en los casos de
estructura segura ya reconocidos. Reproducciones O0/O2 y regresiones dirigidas
en `shared-static-factory-cleanup.md` y `shared-implicit-constructor-cleanup.md`;
las bases no triviales y los campos de valor con drop siguen pendientes.
`a60eb0b64` libera el débil implícito cuando la destrucción del último fuerte
sale por una excepción implícita; pruebas de clases y structs y límites en
`shared-destructor-weak-cleanup.md`.
`49bf24d2a` corrige el conteo débil local consultado reentrantemente durante
la destrucción, con pruebas O0/O2 y controles descritos en
`shared-local-reentrant-weak-count.md`.
`2351dc123` impide que una tupla descarte la marca de un valor prestado de solo
lectura; se rechazaron las reproducciones públicas de `Rc`/`Arc` y pasaron los
controles dirigidos de tuplas y referencias. La regla conservadora y sus límites
están en `readonly-array-borrow.md`.
`eb9f80488` cubre además inicializadores con llaves para tuplas y campos sin
ownership de structs; pasaron las reproducciones dirigidas y los controles de
agregados, con los mismos límites de vida documentados.
`96cbfa053` aplica la decisión del usuario de rechazar `Rc` y `WeakRc` en
almacenamiento estático compartido, incluidos contenedores y derivados
conocidos. Los controles dirigidos y las limitaciones de módulos precompilados
están en `shared-static-storage.md`; no acredita todo el contrato de hilos.
`3ddf6b3c9` rechaza el reemplazo de un propietario local move-only cuando una
vista derivada sigue viva; las pruebas dirigidas y sus límites están en
`move-only-owner-overwrite-borrow.md`.
`7f69e7643` extiende el seguimiento a vistas de parámetros move-only por valor;
sus controles y límites están en `move-only-parameter-view.md`.
`3257699cf` registra la vida de la clase prestada por `Get()` desde un
parámetro move-only por valor; pruebas y límites en
`move-only-parameter-get-borrow.md`.
`ec83e2c35` conserva la vida de la clase obtenida a través de una vista local
de un parámetro move-only; evidencia en `move-only-parameter-view-get.md`.

Las fábricas que propagan excepciones siguen rechazando bases con destructor o
campos que requieren limpieza. La regresión preparada cubre ambas familias,
fallos antes y después de completar la base, recuperación local, herencia de
dos niveles y delegación; pasa análisis semántico y falla en la emisión por
esa restricción. La evidencia y los casos aún no ejecutados están en
`diseno-punteros-y-asan.md`.

**Actualización posterior:** tras adaptar el proyecto mínimo a los traits
`Send` y `Sync` actuales, esa regresión pasó para Rc y Arc en O0/O2 tanto con
Core desde fuentes como con Core binario aislado. Por tanto, el rechazo del
párrafo anterior es un resultado histórico, no el resultado actual de ese
fixture. Los hashes, registros y límites se detallan en
`diseno-punteros-y-asan.md`; no se afirma cobertura de todas las cargas ni de
la liberación física de los bloques.

La fábrica normal también tiene una ruta de limpieza incompleta: si una
inicialización estática falla después de construir una base con destructor,
Rc y Arc capturan la excepción pero no destruyen esa base en O0 ni O2. El
control sin excepción sí la destruye. La reproducción y sus límites están en
`diseno-punteros-y-asan.md`; este caso sigue abierto.

**Actualización del prototipo:** con cambios de construcción todavía sin
commit, 17 casos seleccionados de campos y bases pasan para Rc y Arc en O0/O2
(68 ejecuciones; `shared-constructor-regression-current.log` en la carpeta de
evidencia). El párrafo anterior describe la reproducción anterior al prototipo;
no demuestra que todas las rutas estén corregidas ni habilita las fábricas que
propagan excepciones. Los límites actuales constan en
`diseno-punteros-y-asan.md`.

La prueba `nsc.stage41.arrays-strings-core` inicialmente no enlazó por el codec
de snapshots; `2d6dd44ac` corrige su dependencia, y `ae22f5cdf` hace lo mismo
para siete objetivos relacionados. Una batería dirigida registró 30/33: los
fallos de constantes genéricas, paquetes y delegados se reprodujeron también
en la base independiente, con los mismos diagnósticos. Para ejecutar los
objetivos comparables se aplicaron ajustes de enlace a los tests en el CMake
del árbol base; los fuentes del compilador permanecieron en la revisión
indicada. La selección comparable de 26 pruebas existentes obtuvo 23/26.
Comandos y resultados: `baseline-suite-build.log`, `baseline-suite.log`,
`baseline-comparison-build.log` y `baseline-comparison-tests.log` en
`A:/nsc-fixes-evidencia`.

`6f3182664` corrige el reconocimiento de paquetes y valores enum; `c35229ed5`
mantiene como negativo el índice de matriz que la prueba anterior esperaba
aceptar; `c9a48e2e1` actualiza la comprobación de IR de delegados sin quitar
la verificación de sus argumentos. La batería final seleccionada pasó 36/36,
incluidas las diez regresiones nuevas (`related-tests.log` y
`related-tests.json`). Además pasaron 12/12 pruebas de análisis e IR de las
etapas 42–44 (`cli-suite.log`), dos de ellas también contadas en las 36, y
tres emisiones de objeto con salida explícita en A (`cli-objects.log`). La
primera tentativa de emisión usó una opción de caché que ese comando no admite;
se corrigió la invocación y las tres pasaron.

**Aclaración posterior:** en Windows algunas pruebas declaran éxito aunque
omiten ejecutar programas generados cuando Wine no está disponible. En
particular, `nsc.stage56.c8-type-root` contiene esa ruta; su resultado CTest
anterior no demuestra que sus programas se hayan ejecutado. Una búsqueda
posterior identificó 103 entradas candidatas que comparten 33 fuentes con
mensajes de omisión (`A:/nsc-fixes-evidencia/runtime-skip-candidates.json`);
ese inventario aún requiere separar las ramas realmente ejecutadas y las
pruebas excluidas de interoperabilidad. Los resultados de los ejemplos y
regresiones ejecutados directamente no dependen de esa ruta de Wine.

El análisis `clang-tidy --checks=clang-analyzer-*` de los cuatro últimos
archivos de frontend terminó con código 0; `analyze-final-*.log` incluye
advertencias de padding en encabezados. No se afirma ausencia de advertencias.

## Continuación de la batería

La identidad registrada para el compilador de esta continuación es
`3.0.0+build.20261108T172826Z.7cfd1570e43f8d37`, LLVM 23.1.0; los comandos y
salidas están en `A:/nsc-fixes-evidencia/full-suite-run/`. Tras incorporar
`checked` e `impl` para struct, el compilador usado en las regresiones tiene
identidad `3.0.0+build.20261108T175418Z.32cbf98fdb9cb99b`
(`compiler-identity-latest.log`). El inventario CTest previo a añadir las dos
regresiones nuevas
contiene 704 entradas. Se compilaron 388 objetivos seleccionados tras enlazar
el codec de snapshots a 70 ejecutables (`610adf8bb`). La primera pasada de
387 entradas unitarias terminó con **349 aprobadas y 38 fallidas**
(`unit-results.xml`, `unit-tests.log`). Las omisiones silenciosas de ejecución
en Windows no se detectan como skipped en ese resumen. Es una primera pasada,
anterior a la corrección de extensión, no una batería final aprobada.

Las 13 pruebas comparadas también fallaron en la revisión base; esto no prueba
que todos sus diagnósticos sean idénticos. Tras `8467d6efc`, la prueba de
extensiones pasó y las otras 12 siguieron
fallando (`baseline-compare-results.xml`, `current-compare-results.xml`). Entre
ellos hay cuatro cierres anómalos en pruebas numéricas/vectoriales. El caso
`item347` también falló invocando `nsc.exe` directamente con `-j 1` y caché
desactivada: Windows devolvió `0xC00000FD` durante la generación LLVM del
módulo `code-0005`. Una copia diagnóstica con 64 MiB de reserva de pila
terminó con el mismo código; esto no identifica todavía la causa de la
recursión. Evidencia en `simd-repro/`.

Las diez regresiones específicas del reporte pasaron nuevamente 10/10
(`regression-results.xml`). La selección de línea de comandos pasó 38/38;
las comprobaciones de proyectos obtuvieron 36/37 (la restante sigue esperando
`destruction=dynamic-descriptor`). Las verificaciones LLVM y emisión de
objetos fallaron inicialmente 11/24 por una expectativa de salida antigua;
`fa6dd489c` conserva la comprobación de máquina y obtuvo 24/24 al volver a
ejecutarlas. Son selecciones separadas, no una cuenta global de cobertura.

Para sustituir la omisión de Wine en CC1 se compilaron y ejecutaron los 68
ejemplos seleccionados directamente en Windows, O0 y O2. Inicialmente pasaron
126/136 ejecuciones. Los cuatro proyectos IO que no encontraban la biblioteca
se repitieron con `--library-reference-root A:/nsc-fixes-evidencia/sdk` y
fuentes originales: c24 y c28 pasaron en ambos niveles; c25 y c26 siguen
rechazados por diagnósticos de excepciones y destrucción, sin que se haya
demostrado aún si el defecto está en el ejemplo o el compilador. El caso c69
pasó en ambos niveles tras `8467d6efc`; quedan 132/136 combinaciones
verificadas, contando sus repeticiones exitosas. Los dos proyectos CC1 con
`extern C` se excluyeron. Véanse `cc1-native/results.json`,
`cc1-root-retry/results.json` y `extension-fixed/results.json`.

La primera pasada unitaria creó cachés bajo varios proyectos fuente pese a
configurar temporales en A. Las carpetas creadas exclusivamente durante esa
pasada se trasladaron a `relocated-test-caches/`; se conservaron las carpetas
que mezclaban datos anteriores y las situadas en rutas de fixtures de
interoperabilidad. Debe aislarse explícitamente la caché de cada fixture
antes de repetir toda la pasada. No se modificaron las carpetas de build de
interoperabilidad ni la instalación funcional. La selección inicial compiló
cinco objetos de ejecutables mixtos antes de corregirse el inventario; esto
se detalla en `decisiones-pendientes.md`.

Una auditoría posterior encontró un error de alcance en la selección inicial:
`nsc.ns3.t13.calling-conventions` y `nsc.item345.vectorcall-hva` incluyen
convenciones de interoperabilidad y se ejecutaron en ambas pasadas unitarias,
aunque debían quedar fuera. Sus archivos se limitaron a la copia y build
aislados; ambas pruebas se excluyeron de las siguientes pasadas. Evidencia y
fuentes revisadas: `A:/nsc-fixes-evidencia/interop-extra-candidates.json`.

La segunda pasada, con redirección temporal de 20 cachés de proyecto a A y
restauración de sus ubicaciones previas, obtuvo 349/385 aprobadas y 36 fallidas
(`unit-isolated-results.xml`). La prueba de extensiones y la de enlace
incremental pasaron frente a la primera pasada; dos pruebas con fixtures en
rutas de interoperabilidad quedaron fuera de esta repetición. La exclusión de
las dos pruebas de convenciones se aplicó después para las siguientes pasadas.
Esa pasada es
anterior a los cambios `93d859e31` y `f3b0314c0`, cuyas regresiones dirigidas
pasaron juntas (`struct-tests.log`).

La comparación amplia posterior usó los mismos 383 nombres en ambos árboles,
excluyendo las dos pruebas de convenciones y los dos fixtures de rutas de
interoperabilidad. La base obtuvo 342 aprobadas y 41 fallidas; la rama obtuvo
347 aprobadas y 36 fallidas. Cinco cambiaron de fallo a éxito y ninguna en
sentido contrario; 36 fallaron en ambas, lo que no demuestra por sí solo que
cada diagnóstico sea idéntico. Resultados y diferencias:
`full-suite-run/baseline-full/unit-isolated-results.xml`,
`current-final/unit-isolated-results.xml` y `full-baseline-comparison.json`.
Ambas ejecuciones restauraron sus 20 redirecciones de caché. La base estaba en
`152654e5a0e2d84d4c2f245ea7590488f0497430`; se modificó únicamente su
`CMakeLists.txt` para enlazar ejecutables de prueba, sin cambiar código del
compilador. La primera compilación amplia de la base falló en 70 enlaces por
el codec y la segunda pasó (`baseline-full-build.log`,
`baseline-full-build2.log`). Las huellas de los dos compiladores y ocho
artefactos SDK están en `compiler-file-hashes.json` y `sdk-file-hashes.json`.

La repetición final de los 68 ejemplos CC1 sin `extern C` produjo 132/136
ejecuciones correctas en O0/O2; c25 y c26 siguen rechazados en ambos niveles
(`cc1-final/results.json`). Se ejecutaron además los tres ejemplos Stage 57
que CTest omite en Windows: 6/6 en O0/O2 (`stage57-native/results.json`).
Los cinco ejemplos IO que las pruebas no podían resolver se comprobaron con
`--library-reference-root` al SDK aislado: análisis y MIR 10/10 y ejecución
nativa 10/10 en O0/O2 (`io-example-checks/results.json`,
`io-example-native/results.json`). Esas comprobaciones no convierten las cinco
entradas CTest originales en aprobadas bajo su configuración inicial.

Una repetición CTest IO con enlaces temporales al SDK se interrumpió tras dos
aprobaciones; no hay resultado completo. Se verificó que el proceso terminó,
se restauraron las 25 redirecciones de caché y se retiraron ambos enlaces
temporales. El log parcial está en `io-final/unit-isolated-tests.log`.
Tres controles de depuración se ejecutaron con copias de sus fixtures dentro
de la carpeta de evidencia: inline y ensamblado pasaron; el de ámbitos ejecutó
el programa pero falló por los rangos PDB esperados. La revisión base falló
en el mismo control de rangos (`debug-copies/` y `debug-copies-baseline/`).
Los controles cuyos fixtures contienen `extern C` permanecen excluidos.

**Estado abierto:** faltan pruebas del inventario, la batería completa histórica
no está aprobada y no se ha demostrado ejecución nativa para todos
los casos que CTest omite en Windows. Los 36 fallos de la segunda pasada
unitaria no se han corregido ni clasificado todos contra la base. Los
casos de lenguaje/API pendientes y la evidencia para decidirlos están en
`decisiones-pendientes.md`. No se hizo merge, push ni despliegue.
El usuario indicó posteriormente utilizar siempre compilación incremental y
pruebas dirigidas; esa instrucción rige las verificaciones siguientes.

La comparación de nombres contra `development-cpp-clang` en el repositorio
original, solo en lectura, encontró estos archivos modificados en ambas ramas:

- `CMakeLists.txt`
- `include/nativesharp/Frontend/ProjectGenericModel.h`
- `src/Frontend/HIR/HirBuildCoordinator.cpp`
- `src/Frontend/HIR/HirExpressionMachine.cpp`
- `src/Frontend/MIR/Internal/MirNodeMachine.cpp`
- `src/LLVM/Internal/OopLlvmImplementation.h`
- `src/LLVM/Lowering/OopLlvmExpressionLeaves.cpp`
- `src/LLVM/Lowering/OopLlvmFieldsAndConversions.cpp`

Es coincidencia por archivo, no una predicción de conflictos de líneas. La
comparación de cambios registrados sin commit en archivos rastreados no halló
coincidencias; no evalúa archivos no rastreados. Registro `shared-files.log`.


## Contraste NCSS de la rama (2026-11-09)

La auditoria NCSS incorporo pruebas directas decimal[] y decimal[2] ademas del caso generico dinamico documentado arriba. Con el ejecutable preexistente A:/nsc-fixes-evidencia/build/bin/nsc.exe, runtime generado de la campana y SDK instalado C:/native-sharp/lib/NativeSharp.Core, decimal[] pasa O0/O2; decimal[2] aun rechaza el layout raw C2.1 en ambos modos. El codigo actual contiene el fallback decimal en el camino gestionado de MirNodeMachine, no en lowerRawElementAddress. No se ha demostrado la identidad exacta de ese binario con HEAD; no se declara cierre de L-07.

Los casos de operador sobre structs con parametros in, relacionados con NCSS 12/24, reproducen resultado incorrecto en O0/O2; controles por valor y metodo ordinario con in pasan. Este hallazgo es independiente de las decisiones pendientes sobre temporales in y ++/--.

DeterministicMath.Log no progresa para infinito positivo segun el bucle del SDK; la reproduccion previa uso el SDK instalado. El archivo de la rama y el instalado tienen SHA-256 identico, pero no se ejecuto el SDK de la rama como proyecto. Detalles, controles y limitaciones: A:/native-sharp-reference/validation/ncss-auditoria/HALLAZGOS-LENGUAJE-NCSS-2026-11-09.md y verificacion-nsc-devel-fixes-2026-11-09/resultados-configurados.json. No se modifico el compilador ni el SDK.



## NCSS: Exp con entrada finita representable (2026-11-09)

En el checkout de la rama dd0775e6353c9f096270bae35e645f5cec350624, DeterministicMath.Exp devuelve el maximo double para todo x > 709. Una prueba con el Core de esta rama y nsc del PATH (build hash 4998f7ed435b91d5), sin cache, dio 1.7976931349E+308 para 709.1 en O0/O2, mientras math.exp da 9.082744919406668E+307. El control 709.0 paso en ambos modos. No se construyo nsc de este HEAD; no se ha corregido la biblioteca. Los ejemplos NCSS 21/26/27/29 implementan Exp propio; no se afirma que invoquen este caso del SDK. Fuente, resultados y hashes: A:/native-sharp-reference/validation/ncss-auditoria/sondeo-exp-709-2026-11-09/resultados.json. Clasificacion: defecto de biblioteca con precision/contrato por definir, no ausencia del operador de lenguaje.



## NCSS: correcciones de Exp y Log en arbol de trabajo (2026-11-09)

DeterministicMath.Exp ya no satura a partir de 709: el cambio conserva saturacion al maximo cerca del limite real de double, incluso ante redondeo de la escala. Log detecta infinito positivo antes de normalizarlo. La prueba incorporada nsc.sdk.deterministic-math-boundaries paso en O0 y O2 sin cache mediante su ejecutor CMake, con nsc del PATH y Core fuente de esta rama. Ver A:/native-sharp-reference/validation/ncss-auditoria/verificacion-correccion-math-2026-11-09/cmake-final-result.json, baseline-log.json y Program.ns de la prueba. El nsc del PATH no se identifica como un binario construido desde HEAD; no se instalo SDK ni se prueba precision para todas las entradas.

Los 29 proyectos originales pasaron O0 sin cache con sus referencias originales: A:/native-sharp-reference/validation/ncss-auditoria/suite-estabilidad-2026-11-09/resultados.json. Esto no prueba que consuman el Core modificado. La correccion es del arbol de trabajo, no commit; L-01 y L-07 arreglo fijo siguen pendientes.


## NCSS: subnormales de Exp

Exp(-745) se reprodujo como cero en O0/O2 con controles positivos; el arbol de trabajo ajusta el limite inferior y evita rondas subnormales intermedias. La regresion permanente ampliada paso O0/O2 sin cache. Evidencia: A:/native-sharp-reference/validation/ncss-auditoria/sondeo-exp-subnormal-2026-11-09/resultados.json y verificacion-correccion-math-2026-11-09/cmake-subnormal-result.json. No se certifican todos los modos de denormales.


## NCSS: cambio preparado decimal fijo, no verificado

lowerRawElementAddress incluye en el arbol de trabajo el fallback decimal Windows x64 usado en arreglos gestionados. Se agregaron tests/integration/decimal_fixed_array y nsc.compiler.decimal-fixed-array. El nsc del PATH anterior al cambio falla en O0/O2; todavia no se construyo ni ejecuto un compilador desde este checkout. L-07 fijo permanece NO VERIFICADO: A:/native-sharp-reference/validation/ncss-auditoria/decimal-fijo-rama-2026-11-09/baseline-path.json.

La regresion DeterministicMath paso O0-O3 en fp-mode precise y deterministic, con fp-denormal preserve (8 combinaciones, sin cache). Evidencia: A:/native-sharp-reference/validation/ncss-auditoria/verificacion-correccion-math-2026-11-09/cmake-all-modes-result.json; no cubre ftz/daz.


## NCSS: regresion de operador in con controles

Se agregaron tests/integration/operator_in_aggregate y nsc.compiler.operator-in-aggregate. En el nsc del PATH el fixture compilo y salio 4 en O0/O2 despues de superar controles por valor, struct pequeno y metodo in; el ejecutor CMake captura la falla. Evidencia en A:/native-sharp-reference/validation/ncss-auditoria/operator-in-rama-2026-11-09/. No se ha modificado el lowering L-01 ni probado un compilador construido de HEAD.


La regresion operator_in_aggregate ahora incorpora controles de ref, out, copia por valor y argumentos in nombrados en orden inverso sobre un agregado de 16 bytes. Con nsc del PATH esos controles pasan antes del fallo existente de operador in (salida 4 O0/O2). Evidencia: A:/native-sharp-reference/validation/ncss-auditoria/operadores-in-controles-adicionales/resultados.json y operator-in-rama-2026-11-09/baseline-extended-path.json. No se ha verificado un compilador de esta rama.

Fixture operator_in_aggregate: structs separados por archivo; resultados vigentes O0/O2 en A:/native-sharp-reference/validation/ncss-auditoria/operator-in-rama-2026-11-09/baseline-separated-path.json. El resultado 4 permanece pendiente de correccion.


L-01: se reprodujo el error de operador in con struct de dos double en locales, campos y elementos de arreglo; controles equivalentes por valor pasaron en O0/O2 con nsc del PATH. Se ampliaron los casos de la regresion permanente. Evidencia: A:/native-sharp-reference/validation/ncss-auditoria/operadores-in-ubicaciones/resultados.json y operator-in-rama-2026-11-09/baseline-locations-path.json. No demuestra estado del compilador reconstruido.

El cambio preparado en MirNodeMachine.cpp paso comprobacion sintactica clang-cl /Zs por PATH; no se ha validado su comportamiento. A:/native-sharp-reference/validation/ncss-auditoria/decimal-fijo-rama-2026-11-09/syntax-path.json.


L-01: las sobrecargas de multiplicacion entre struct de dos double y escalar fallan con parametro struct in en ambos ordenes, mientras los controles por valor pasan O0/O2 con nsc del PATH. Se incorporaron ambas al fixture permanente. Evidencia: A:/native-sharp-reference/validation/ncss-auditoria/operadores-in-escalar/resultados.json; no demuestra estado de compilador reconstruido.

El cambio preparado en MirNodeMachine.cpp compilo a objeto con clang-cl por PATH y fuentes estables; no se enlazo ni ejecuto el compilador. Evidencia A:/native-sharp-reference/validation/ncss-auditoria/decimal-fijo-rama-2026-11-09/object-compile-path.json. L-07 fijo permanece NO VERIFICADO.


L-07: controles independientes int[2], double[2] y decimal escalar pasan O0/O2 con nsc del PATH; decimal[2] falla compilacion. Se ampliaron controles de la prueba permanente. MirNodeMachine.cpp modificado compilo a objeto con clang-cl PATH, pero no se enlazo ni probo compilador de la rama. Evidencia A:/native-sharp-reference/validation/ncss-auditoria/decimal-fijo-controles/resultados.json y decimal-fijo-rama-2026-11-09/object-compile-path.json. Estado: NO VERIFICADO.


L-01: cambio provisional en llamadas estaticas de operadores de usuario usa parameterPassKinds para evitar copiar direcciones de argumentos in; source OopLlvmCalls.cpp compilo a objeto con clang-cl PATH. No se enlazo ni probo compilador reconstruido; estado NO VERIFICADO. Se agrego control de comparaciones struct de dos double: la prueba conjunta ==/!= falla con in, pasa por valor en O0/O2 de nsc instalado. Evidencias A:/native-sharp-reference/validation/ncss-auditoria/operator-in-rama-2026-11-09/object-compile-path.json y operadores-in-comparacion/resultados.json.


L-01: a+a con struct de dos double falla con operador in y pasa por valor O0/O2 en nsc del PATH; caso agregado al fixture. Se trazaron estaticamente la evaluacion fuente de DirectStaticCall, la llamada sintetica de Result sin memberOrdinal y la marca de metadata de operador. Evidencia A:/native-sharp-reference/validation/ncss-auditoria/operadores-in-alias/resultados.json y l01-flujo-operando-rama-2026-11-09.json. El cambio continua NO VERIFICADO.


L-08: stackalloc decimal[2] se rechaza por ausencia de value layout con nsc del PATH O0/O2; control double equivalente pasa. Las dos rutas HIR consultan valueLayoutForTypeKey y la ruta MIR de raw storage consulta elementLayout. La correccion provisional de decimal[2] no cubre esas rutas. Se agrego nsc.compiler.decimal-stackalloc; permanece ABIERTO. Evidencia A:/native-sharp-reference/validation/ncss-auditoria/decimal-stackalloc-controles/resultados.json y fixture-baseline.json.


L-08: tratamiento provisional decimal localizado en dos rutas HIR y MIR StackAllocation, sin cambiar el servicio compartido. Los tres C++ compilaron a objeto por PATH, sin enlace o ejecucion del compilador; NO VERIFICADO. Reproduccion dinamica con count variable: double pasa, decimal falla CC1 en O0/O2 con nsc instalado. Evidencia A:/native-sharp-reference/validation/ncss-auditoria/decimal-stackalloc-controles/object-compile-path.json y dynamic-baseline.json.


## NCSS: validacion con compilador reconstruido

Con nsc reconstruido, build hash 59a1b13ae84a9cc9 y SHA-256 417c7ffe8c7759e8c1fe9659a9b0e3c4845de000de331abecc507a9dd1e3e5eb, pasaron los fixtures decimal fijo, stackalloc decimal, operadores in y DeterministicMath, ademas de 34 combinaciones historicas O0/O2 de L-01/L-07. Los 29 proyectos NCSS originales compilaron y ejecutaron O0/O2 sin cache (58/58), con sus referencias al Core instalado. El fixture matematico utiliza Core fuente de la rama. Evidencia A:/native-sharp-reference/validation/ncss-auditoria/verificacion-nsc-devel-fixes-2026-11-09/current-gates.json, current-historical-gates.json y suite-nsc-fixes-2026-11-09/resultados.json. No se certifica ABI general, metadata precompilada ni otros targets; cambios sin commit.
