# Host.Driver — orquestación, CLI, productos, paquetes y contratos operativos

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones expresamente registradas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 51 archivos, 15,323 líneas y 675,413 bytes.
- **Inventario:** 387 símbolos físicos, 77 contratos y 616 casos diseñados.
- **Validación realizada:** lectura completa de 26 headers y 25 implementaciones; índice AST de definiciones,
  despacho de comandos, formatos, estados, errores, transacciones, concurrencia y segunda pasada independiente.
- **No realizado:** build, ejecución, baseline, cobertura, instalación, despliegue o modificación del producto.

## 2. Frontera del componente

Host.Driver convierte argumentos y estado externo en sesiones de compilación observables. Posee la política de CLI,
selección de fases, perfiles, concurrencia, progreso, generación de productos, consumo de metadata, validación de
artefactos, paquetes, instalación y documentos operativos. Coordina Host.Compilation, Host.Frontend, Host.LLVM,
Host.Linker, Host.Interop y Host.Runtime; no redefine sus algoritmos internos.

`Driver.cpp` concentra tanto el despacho como puentes de metadata y publicación. Esa concentración es una propiedad
de la revisión, no un contrato de prueba: los casos futuros se atan a entradas y resultados, de modo que una
modularización posterior no obligue a reescribirlos.

## 3. Grafo operativo

El flujo principal es:

1. `CompilerConsole` extrae idioma y conserva los argumentos restantes;
2. `Driver::run` normaliza opciones globales y crea scopes restaurables;
3. comandos de proyecto se delegan a `ProjectCommands`;
4. comandos directos seleccionan frontend, fase de proyecto o generador;
5. generación OOP produce objetos, enlaza, verifica y publica mediante transacción;
6. metadata, headers, runtime y PDB se publican sólo después de validar el conjunto completo.

La jerarquía de fases de proyecto arrastra únicamente prerequisitos: sintaxis, genéricos, nombres, miembros,
herencia, interfaces, constraints, cuerpos, despacho virtual, destrucción, construcción, layouts, HIR, MIR y LLVM.
El comando de constraints tiene frontera propia y no analiza cuerpos por accidente.

## 4. Sesión, tiempo, progreso y cancelación

`DriverExecutionScope` instala por hilo opciones de proyecto, loader, política de jobs, deadline, presupuesto O2,
progreso y estadísticas. El destructor restaura los punteros previos y emite tiempo/presupuesto cuando aplica.
`DriverPhaseTimer` registra duración aun por salida excepcional.

Los eventos de progreso tienen renderer de texto o JSON estable. El trace de tiempos agrega fases conocidas,
mantiene valores cero cuando una línea es ilegible y añade working set máximo en Windows. El timeout es cancelación
cooperativa en fronteras de fase, no interrupción asíncrona de código arbitrario.

## 5. Jobs, scheduler y worker pool

`CompilationJobPolicy` convierte hardware cero en uno, reserva cores sin dejar capacidad cero, permite selección
automática o explícita y limita el valor solicitado a lo disponible. Su scope es thread-local y anidable.

`CompilationGraphScheduler` sólo admite dependencias hacia ordinales ya existentes. Ejecuta waves listas ordenadas
por clave estable y ordinal, y reporta dependencias no resueltas o fallos de workers. El grafo público no puede crear
un ciclo directo porque cada arco apunta hacia atrás; la rama de ciclo se conserva como defensa.

`CompilationWorkerPool` numera tareas, publica índice de worker thread-local, elige como causa la excepción de menor
ordinal y cancela/descarta la cola de forma determinista. `wait` une la frontera lógica, restablece cancelación y
permite reutilización; el destructor detiene y une los threads.

## 6. Perfiles y consola

Los perfiles `debug` y `release` resuelven defaults de optimización y depuración; flags explícitos los sustituyen.
La consola acepta `--language value` y `--language=value`, rechaza duplicados, faltantes o idiomas desconocidos y
retira sólo esas opciones. Localización y banner dependen de un scope thread-local restaurable.

`CrashBoundary` traduce excepciones C++, fallos de plataforma y retornos normales a diagnósticos/códigos de salida
sin dejar escapar una excepción a `main`.

## 7. Compatibilidad y deprecación

La política `NSCOMPAT` exige encabezado y campos conocidos con valores normativos para capacidades instaladas; el
lector general conserva campos adicionales. `checkCompilerCompatibility` informa compatibilidad y no rechaza por
etiqueta de versión. La matriz y el ledger tienen render, escritura transaccional y verificación separados.

La tabla de deprecación contiene siete features. El parser acepta `major.minor` estricto; la evaluación distingue
anuncio, warning, error y retiro. Un retiro anticipado sólo es permitido por excepción de seguridad/UB.

La CLI, ayuda, verificador de instalación y documentos de release aún exponen nombres 2.0/3.0, stages y artefactos
versionados. Se conserva como contradicción histórica; esta fase no lo legitima ni lo corrige.

## 8. Argumentos y despacho

Las opciones globales pueden aparecer en cualquier posición y se extraen antes del despacho. Incluyen
optimización, debug, FP, LTO, incremental, profile, jobs, cache, raíz de bibliotecas, presupuestos genéricos,
timeout, progreso, logging, bundles, subsystem y bibliotecas adicionales. Duplicados, rangos inválidos y opciones
incompatibles fallan en la frontera propietaria.

El despacho se decide por aridad y acción. Cada combinación no reconocida termina en `CommandLineError`; una acción
reconocida propaga el código de su productor. La ayuda enumera la superficie vigente, pero no sustituye los
contratos individuales.

## 9. Frontend de archivo único

La ruta de archivo único carga, tokeniza, parsea y produce los modelos disponibles. Los dumpers sólo emiten cuando
su modelo existe; los verificadores HIR/MIR/CFG/Dominance/SSA/optimization/LLVM propagan su diagnóstico exacto.
Objeto y assembly crean target Windows x64, optimizan según nivel, emiten y verifican COFF o tamaño no vacío.

La generación de ejecutable exige un `int Main()` válido, configura runtime y subsystem, y publica el PE sólo si
el builder termina. Esta ruta es distinta de la compilación OOP de proyecto.

## 10. Productos OOP de proyecto

Todos los productos OOP compilan hasta MIR, validan modelos requeridos, generan objetos por unidades, completan la
fase de codegen y publican mediante `ArtifactTransaction`.

- objeto: objeto COFF verificado, nivel y debug observables;
- ejecutable: exactamente un entry point, imports, libraries, runtime closure, PDB e incremental state;
- estática: archive, header y NSMETA en una sola transacción;
- DLL: export set derivado de superficie pública, import library, header, NSMETA, runtime y PDB coherentes.

Un fallo de cualquier companion impide publicar el conjunto parcial.

## 11. Exportación de DLL

Se exportan constructores publicables, métodos públicos/protected pertinentes, namespace functions públicas,
accessors de descriptors, drop glue de structs y accessors de estado no const. Se excluyen lambdas, abstractos,
compiler-generated no publicables, imports y miembros de dependencias. El conjunto se ordena y deduplica.

Una DLL sin símbolos exportables todavía es válida si posee superficie pública sólo de metadata; sin ninguno de
los dos contratos se rechaza.

## 12. Conversión de metadata a fuente

El puente NSMETA reconstruye tipos builtin, referencias, ownership, lifetimes, genéricos, specialization keys,
accesibilidad, defaults, constants, bitfields, fields, properties, indexers, events, constructors, destructors,
operators, conversions, delegates, enums, records, variants, unions, interfaces, traits, namespace functions y
states.

La fuente generada se vuelve a parsear. El resultado sólo es aceptado si conserva la clausura pública esperada y
no inventa tipos. Format-minor gates preservan compatibilidad con productores históricos; son evidencia vigente,
no una recomendación para pruebas nuevas.

## 13. Templates genéricos precompilados

La fuente de templates se construye aparte, conserva constraints, parámetros, defaults, lifetimes y cuerpo
publicado, y se verifica mediante parser. Metadata pública y templates son módulos sintéticos diferentes porque su
frontera de materialización y diagnóstico es distinta.

## 14. Clausura precompilada

El driver carga metadata referenciada, verifica identidad/target/ABI, descubre referencias transitivas junto a
artefactos, agrega nodos sintéticos al grafo y sustituye un binario cuando existe el proyecto fuente canónico del
mismo módulo. Source tiene precedencia; no se enlazan dos definiciones de un módulo.

La validación exige que cada tipo externo publicado sea provisto por source, peer metadata o dependencia declarada.
Los módulos se materializan en orden estable antes de iniciar el frontend.

## 15. Bibliotecas e imports

Cada `LibraryReference` valida archive y NSMETA; una DLL opcional debe concordar exactamente con los símbolos
requeridos. Imports C estáticos se agrupan por DLL y símbolo. Colisiones que requieren resolución runtime no crean
dos entradas PE: agregan `LoadLibraryA` y `GetProcAddress` una sola vez.

Las libraries explícitas, heredadas y de CLI forman el conjunto efectivo del linker. Una referencia inválida falla
antes del enlazado.

## 16. Runtime closure

El empaquetado comienza en las DLL declaradas, recorre imports hermanos, evita PATH y acepta una colisión de nombre
únicamente cuando el contenido es idéntico. Copia a staging y elimina lo copiado si la clausura falla. La transacción
superior convierte cada runtime staged en salida final sólo después de validar PE y link.

## 17. API, ABI y conjunto de artefactos

NSMETA puede verificarse, imprimir contrato ABI, superficie source/ABI y fingerprints. Los diffs clasifican cambios
source y ABI; el código de salida falla cuando la compatibilidad correspondiente no se conserva.

La verificación conjunta exige archive COFF con símbolos requeridos, DLL con export table exacta, PDB cuya GUID/age
coincide con RSDS, basename portable y streams de módulos/publics/sources/checksums. Después valida que la API pública
pueda materializarse como fuente.

## 18. JSON y sidecar de comandos de proyecto

El parser JSON local impone 4 MiB, claves únicas, strings/escapes, arrays, objects, bool, null y números. No valida
surrogates ni usa un parser general. `parseValue` llama recursivamente a objects/arrays y queda en contradicción con
la prohibición normativa de recursión.

`.nspm.json` exige schema 1, build settings y packages únicos case-insensitive. El renderer ordena campos y produce
JSON determinista. `CommandOutput` ofrece schemas text/JSON paralelos con mismo código de salida.

## 19. New, paquetes y restore

`new` valida identificador/template, crea `.nsproj`, sidecar y `Main.ns` sin sobreescribir configuración salvo
`--force`. Los packages provienen de manifest explícito o convención exacta de `.lib/.nsmeta/.dll`; toda ruta debe
quedar confinada bajo la raíz canónica y cada artefacto se fija por SHA-256/tamaño.

Restore ordena packages, genera lock determinista, verifica store actual y publica mediante staging/backup/rename.
`--locked-mode`, `--verify` y `--force` tienen combinaciones cerradas. Un fallo restaura backup o deja el estado
anterior; no mezcla contenido nuevo y antiguo.

## 20. Build, run, clean y package CRUD

Build combina CLI, project settings y sidecar con precedencia explícita; sólo admite NSLink, genera un proyecto
temporal con referencias del lock, valida el grafo, aplica `--precompiled-only`, invoca el driver y publica Content
transaccionalmente. El reporte de dependencias fingerprinta source, project references, libraries y store.

Run reutiliza build y en Windows crea el proceso con quoting compatible, UTF-8 validado, working directory del
artefacto y código de salida propagado. Clean limita borrado a `bin`, `obj` y `.nspm.*.nsproj` de la raíz inferida.
Add/remove/list sólo modifica el sidecar transaccionalmente y conserva orden.

## 21. Instalación y lifecycle

El manifest de instalación usa paths relativos canónicos, inventario ordenado, tamaño y SHA-256 lowercase; rechaza
su propio archivo y limita 10,000 entradas/16,384 bytes por línea. La verificación autentica cada entrada y busca
residuos sólo en namespaces administrados seleccionados.

El verificador de entorno exige un conjunto versionado de ejecutables, contratos y documentos. Lifecycle compara
manifests ignorando únicamente identidades de build volátiles, deriva planes add/keep/remove/replace y aplica
remoción sólo sobre paths propios, no symlink y sin modificación. Sólo poda directorios vacíos del paquete.

## 22. Migración y documentos operativos

El analizador de migración tokeniza comentarios/strings, procesa suppressions y emite hallazgos para null,
ownership, moves, using, narrowing y orden de cleanup. Los fixes sólo se aplican cuando están marcados seguros, con
prueba de equivalencia y rangos no solapados.

La guía ejecutable se valida mediante su estructura pública: encabezado NativeSharp 2.x, estrategia incremental,
inventario de decisiones manuales y capítulos estables. Una entrada sin hallazgos conserva un único capítulo
`VERIFY`; una entrada con transformaciones publica pares `Before:` y `After:`. Ningún sentinel privado forma parte
del contrato observable.

Baseline, guide, compatibility matrix, release freeze, final baseline, promotion act, numeric contract y toolchain
contract poseen render canónico, escritura transaccional y verificación. Son formatos operativos distintos; una
prueba no debe aceptar uno por parecerse textualmente a otro.

## 23. Estados, invariantes y errores

1. Scopes thread-local siempre restauran el valor anterior.
2. Collections que alimentan artefactos, reports o locks se ordenan y deduplican.
3. Un productor fallido impide publicar consumers y companions.
4. Paths externos se canonicalizan y confinan antes de copiar, borrar o hashear.
5. Hash, tamaño, ABI, imports, exports y PDB se verifican en la frontera propietaria.
6. JSON y manifests rechazan duplicados y estructuras desconocidas donde el schema es cerrado.
7. Los códigos distinguen comando inválido, compilación y fallo interno.
8. Una transacción no presenta una mezcla de revisión anterior y nueva como éxito.

Los errores se propagan con contexto del productor. Un fallo derivado no debe sustituir la causa primaria de menor
ordinal o la validación más temprana.

## 24. Ownership, concurrencia y complejidad

RAII gobierna scopes, timers, temporales, handles y transacciones. Worker pool y scheduler poseen sus colas y
threads; lambdas reciben referencias sólo durante la vida de la sesión síncrona. Los productos usan `unique_ptr` y
valores para expresar ownership.

Sorting y hashing dominan manifests/artefactos; grafos y clausuras usan sets/maps de visitados. Sin embargo,
`canMaterializeContractType`, `collectMaterializedContractTypes`, emisión de tipos anónimos, clausura de callees y el
parser JSON conservan recursión. Se documenta en `CON-0008`; reemplazarla requiere otra fase funcional.

## 25. Estrategia futura de pruebas

Los contratos se probarán mediante argumentos mínimos, streams, filesystem temporal, loaders/builders inyectables y
artefactos sintéticos estrictamente pequeños. Cada contrato recibe nominal mínimo, representativo, prerequisito
ausente, estructura inválida, límites/seguridad, atomicidad de fallo, determinismo y escala/adversarial.

Los oráculos observan códigos, modelos, orden, contenido, hashes, paths y ausencia de publicación parcial. No
observan cantidad de `if`, nombre de helper, línea ni que `Driver.cpp` siga conteniendo la lógica. Los productos que
requieren toolchain real quedan en integración; profundidad, grandes clausuras y concurrencia hostil son `NIGHTLY`.

## 26. Reconstructibilidad y cierre

La documentación responde propósito, entradas, salidas, contratos, estructuras, algoritmos, invariantes, errores,
estados, ownership, integración, formatos, complejidad, decisiones vigentes, limitaciones y estrategia de prueba.
Los 51 archivos y 387 símbolos se trazan a 77 contratos con 616 casos.

`Host.Driver` queda `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Esto no afirma ejecución, cobertura,
aprobación funcional de artefactos históricos ni ausencia de defectos de producto.
