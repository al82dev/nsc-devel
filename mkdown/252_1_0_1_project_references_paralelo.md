# Entrega 252 — ProjectReferences en paralelo / Parallel project references

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 252.
- Resultado / Result: referencias de proyecto independientes se cargan en paralelo con el límite
  efectivo de `-j`, mientras el grafo final conserva exactamente el orden serial determinista.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`ProjectGraphLoader` now discovers the graph in canonical path-sorted frontiers. The root descriptor
forms the first frontier; every loaded descriptor contributes its previously unseen references to
the next frontier. All descriptors in one frontier are independent discoveries and execute through
the bounded `CompilationWorkerPool`. Results are consumed only after the complete frontier finishes
and always in canonical order.

La detección de módulos duplicados, la deduplicación por ruta y la expansión de nuevas referencias
ocurren en el thread coordinador. Después del descubrimiento, un recorrido DFS separado reconstruye
el orden topológico tradicional, detecta ciclos con la misma cadena de módulos y deja el proyecto
raíz al final. Por ello, terminar antes en un worker nunca cambia ordinals ni diagnósticos.

`ProjectCompilation` receives the effective job count selected by the Driver. The precompiled
metadata scan and the explicit graph commands receive the same policy. `--print-job-policy` now
reports `scheduler: project-references`, accurately describing the first production phase connected
to parallel execution without claiming that subsequent frontend/backend phases are parallel yet.

Para evitar un ciclo de bibliotecas, las implementaciones existentes de la política y del pool se
enlazan desde `nsc_compilation`; sus headers y namespace de Driver permanecen compatibles. Frontend
depende de Compilation, mientras Driver continúa dependiendo de Frontend, sin una arista inversa.

## Archivos / Files

- `include/nativesharp/Compilation/ProjectGraphLoader.h`: worker count and injectable loader contract.
- `src/Compilation/ProjectGraphLoader.cpp`: concurrent frontier discovery and deterministic DFS.
- `include/nativesharp/Frontend/ProjectCompilation.h` and `src/Frontend/ProjectCompilation.cpp`:
  effective job-count propagation into graph loading.
- `src/Driver/Driver.cpp`: active policy propagation and honest scheduler report.
- `tests/unit/ProjectGraphTests.cpp`: controlled concurrency and serial/parallel equivalence.
- `tests/unit/DriverTests.cpp`: scheduler-report contract.
- `CMakeLists.txt`: pool and policy implementation placement in the shared compilation layer.
- Compiler contract, conformance map and 2.0 roadmap: normative state and traceability.

## Decisiones / Decisions

1. Paralelismo por fronteras: no se ejecuta un descriptor antes de descubrirlo desde la ola previa.
2. Mutación centralizada: workers sólo producen un resultado en su slot preasignado.
3. Orden físico separado del lógico: el DFS canónico decide el grafo, no el orden de terminación.
4. `ProjectLoadFunction` inyectable: permite probar concurrencia real sin depender de sleeps o tamaño.
5. Un worker conserva una ruta válida y produce exactamente la misma salida que varios workers.

## Aceptación / Acceptance

- Dos referencias hermanas se observan activas al mismo tiempo en un pool de dos workers.
- La prueba tiene timeout acotado y falla, en vez de colgarse, si el trabajo se vuelve serial.
- Los dumps producidos con uno y dos workers son idénticos.
- Diamantes, ciclos, módulos duplicados y referencias transitivas faltantes conservan su contrato.
- `-j` y núcleos reservados determinan la capacidad usada por la carga real del grafo.
- Pruebas enfocadas y reconstrucción limpia de `nsc` pasan; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Carga ProjectReferences en paralelo / Load ProjectReferences in parallel`

**Descripción / Description**

`Discover independent project-reference descriptors through canonical parallel frontiers, consume
worker results in stable order, preserve deterministic DFS ordering and diagnostics, propagate the
effective job policy into every production graph-loading path, and prove serial/parallel graph
equivalence with a controlled focused test.`
