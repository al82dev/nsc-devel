# Campaña de limitaciones del compilador y SDK

Base: `152654e5a0e2d84d4c2f245ea7590488f0497430`, rama
`development-fixes-limitaciones`. Fuentes: reporte y registro detallado de
`A:/native-sharp-reference`. Compilador y runtime compilados desde la copia en
`A:/nsc-fixes-evidencia/build`; SDK de la copia en `A:/nsc-fixes-evidencia/sdk`.
Windows x64, LLVM/Clang 23.1.0, host estático y Release. Se usa el enlazador
externo `C:/native-sharp/bin/nslink.exe` como dependencia de referencia y el
runtime generado en A. El ejecutable final informa NativeSharp 3.0.0 y build
hash `c237186aba0246a4` (`final-nsc-version.log`). No se instala ni modifica
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
| Rc / WeakRc; Arc / WeakArc | `sdk-01`: nombres no encontrados. El usuario autorizó diseñar e implementar las cuatro familias; implementación y validación pendientes. El contrato impreso no demuestra una implementación. |
| Dictionary string AddOrUpdate | `sdk-02`: termina 3221226356; doble liberación. Corregido en `5127e4d07`, con ejecución de regresión. |
| SortWith | `sdk-03`: nombre no encontrado. La sobrecarga vigente `Sort<T>(Span<T>, Comparison<T>)` cubre la ordenación con comparador; una copia de `c27_algoritmos` se compiló y ejecutó nativamente en O0/O2 con salida 42 (`A:/nsc-fixes-evidencia/sort-existing/result.json`). El nombre `SortWith` no existe y no se agregó. |
| Saturación: suma y conversiones a short y byte | `comp-01`: `-2147483648`, `-25536`, `34`; corregido para esas operaciones en `a7436961d` y `bb4c18900`. No se afirma cobertura de todas las operaciones saturadas. |
| Literales y cálculo decimal | `comp-02`: factura 101, comparación falsa, redondeo 100/100/101. `a63333883` conserva el lexema de literales y corrige la reproducción; no demuestra precisión completa de decimal ni inicializadores estáticos. |
| checked i128 | `comp-03`: la suma desbordaba y continuaba con wrapping. El usuario eligió trap; `93d859e31` corrige la suma firmada i128 con regresiones O0/O2. Otras operaciones no quedan cubiertas. |
| División i128 | `comp-04`: enlace falla por `__divti3`. `2f1ae8d47` corrige división firmada; pruebas diferenciales, extremos y divisor cero. Resto y unsigned no están cubiertos. |
| sealed O2 | `43-dispatch`: IR optimizado conserva llamada indirecta. Pendiente decidir el objetivo de optimización. |
| Cancelación async | `comp-06`: traza 0 y salida 5. Con un avance adicional de `MoveNextAsync`, `comp-06-deferred` da traza 7 y salida 0. El usuario confirmó limpieza en el siguiente avance, conforme al contrato existente. |
| impl externo para struct | `68-struct` se rechazaba. El usuario autorizó despacho estático sin boxing; `f3b0314c0` lo implementa y añade regresión. |
| Delegado genérico con retorno T | `69-delegate`: rechazo de T. `3c03a4440` corrige el patrón reportado; otras formas de retorno genérico no están demostradas. |
| Restricción entre parámetros genéricos | `69-constraint`: resolución no encuentra T. Pendiente investigación y decisión del contrato de restricciones. |
| Lambda sin captura en Main global | `69-lambda`: falla en HIR por ordinal de owner. `f8a05890f` corrige función global sin captura; cierres con captura no están cubiertos. |
| Matriz dinámica genérica decimal | `70-decimal`: falta layout del elemento. `de6415700` añade el layout en operaciones de arreglo; reproducción y regresión pasan. |
| Método genérico en matriz genérica | `70-method`: se rechaza deliberadamente como no implementado. Pendiente diseño. |
| Función genérica con dimensiones const reenviadas | `70-static`: rechazaba Rows. `690a8ffd3` corrige el caso y verifica multiplicación y argumentos inválidos. No cubre todos los ámbitos o paquetes. |
| Temporal como argumento in de operador | `70-temporary`: rechazo de argumento. Pendiente decisión de lenguaje y duración. |
| Llamada a función desde #run | `73-run`: rechazo del evaluador limitado. Pendiente decisión de alcance del evaluador. |
| Inspección general de campos | `74-fields`: ejecuta un ejemplo explícito, no un inspector universal. No se localizó API general; pendiente diseño. |
| derive Print | `75-derive`: texto `GridPoint`, sin campos. Pendiente decisión de formato. La igualdad falsa observada en la misma ejecución se corrigió de forma acotada; véase `derive-ignore-equality.md`. |
| SIMD máscara a vector, indexación y condicional | `89-mask-convert`, `89-mask-index`, `89-mask-select`: rechazos reproducidos. Pendiente definir las operaciones y representación deseadas. |
| AddressSanitizer | No se localizó opción en `nsc --help`. La clase `LlvmNativeSanitizer` instrumenta IR; su prueba existente pasó. Una reproducción exploratoria de IR instrumentado con `opt` y enlazado con Clang sí diagnosticó escritura fuera de límites (`asan-probe/run.log`). El usuario aprobó una opción explícita; integración en nsc/NSLink pendiente. |
| Operadores ++ / -- | `39-increment`, `39-decrement`: parser rechaza la sobrecarga. Pendiente decisión sobre semántica y evaluación. |
| NativeSharp.Text | `text-namespace`: namespace no encontrado. Pendiente decisión de API. |
| Compile con .. | `102-outside`: proyecto rechaza rutas fuera de su raíz. Pendiente decisión de resolución de proyectos. |
| Ejemplo 66 | Antecedente ya resuelto en el ejemplo; no se atribuye un defecto vigente al compilador. |

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

La segunda pasada, con redirección temporal de 20 cachés de proyecto a A y
restauración de sus ubicaciones previas, obtuvo 349/385 aprobadas y 36 fallidas
(`unit-isolated-results.xml`). La prueba de extensiones y la de enlace
incremental pasaron frente a la primera pasada; dos pruebas con fixtures en
rutas de interoperabilidad quedaron fuera de esta repetición. Esa pasada es
anterior a los cambios `93d859e31` y `f3b0314c0`, cuyas regresiones dirigidas
pasaron juntas (`struct-tests.log`).

**Estado abierto:** faltan pruebas del inventario, la batería completa
pertinente no está aprobada y no se ha demostrado ejecución nativa para todos
los casos que CTest omite en Windows. Los 36 fallos de la segunda pasada
unitaria no se han corregido ni clasificado todos contra la base. Los
casos de lenguaje/API pendientes y la evidencia para decidirlos están en
`decisiones-pendientes.md`. No se hizo merge, push ni despliegue.

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
