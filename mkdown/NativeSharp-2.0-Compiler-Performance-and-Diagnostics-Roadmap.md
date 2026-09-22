# NativeSharp 2.0 — Rendimiento y diagnósticos del compilador / Compiler Performance and Diagnostics Roadmap

> **Estado:** plan oficial previo a NativeSharp Language 3.0  
> **Base:** NativeSharp 2.0.0, ABI Win64 preservada  
> **Alcance:** NSC, backend LLVM, integración NSLink/PDB, empaquetado y experiencia de diagnóstico  
> **No agrega:** sintaxis, semántica de lenguaje ni cambios ABI incompatibles
> **Desbloquea:** [NativeSharp-Language-3.0-Roadmap.md](NativeSharp-Language-3.0-Roadmap.md), Tanda `NS3-T00`

---

## Propósito / Purpose

Esta fase pertenece al compilador y al toolchain, no agrega sintaxis ni modifica la semántica del lenguaje. Es, sin embargo, un **requisito de entrada bloqueante** para NativeSharp Language 3.0. No puede abrirse `NS3-T00` mientras este gate no esté certificado.

El objetivo es que NativeSharp no solo produzca programas nativos rápidos: el propio compilador debe compilar con rapidez predecible, mostrar qué está haciendo y dejar evidencia autocontenida cuando falle. Los niveles `-O0`, `-O1`, `-O2` y `-O3` ya existentes deben convertirse en contratos diferenciados, medidos y verificables; no basta con mapear cuatro nombres a niveles del backend.

## Convenciones de ejecución / Execution conventions

- Programa: `NS2-PERF`.
- Tanda: `NS2-PERF-PNN`.
- Commit lógico: `nsc2-perf(pNN): <slug>`.
- Benchmark y evidencia: `tests/performance/pNN/` y `_nscbuilder/performance/`.
- Baseline: `nsc-2.0-perf-pNN-baseline`.
- Cada tanda termina con su commit de tanda y un baseline nuevo comparado contra el anterior.
- No se mezclan cambios de semántica, features 3.0 ni refactors sin medición.

Cada resultado conserva como mínimo: revisión de NSC/NSLink/LLVM, máquina de referencia, configuración, corpus, comandos exactos, número de repeticiones, mediana, percentiles acordados, CPU, memoria máxima, I/O, tamaño de artefactos y diferencias contra el baseline anterior.

## NS2-PERF-P00 — Baseline reproducible y atribución de costo / Reproducible baseline and cost attribution

**Commit:** `nsc2-perf(p00): reproducible-performance-baseline`

- **Implementación:** crear un runner hermético para medir proyecto mínimo, proyecto mediano, juego real, bindings, solución multi-proyecto, carga intensa de genéricos y build con PDB. Ejecutar clean, no-change, cambio privado de un archivo y cambio de API pública bajo `-O0` a `-O3`, `-g0`, `-gline-tables-only` y `-g`, con NSLink y `lld-link` donde aplique.
- **Instrumentación:** separar startup, descubrimiento del toolchain, lectura del proyecto, parse, binding, tipos, análisis de ownership/lifetimes, MIR, optimización, codegen, PDB, enlace, empaquetado y publicación.
- **Pruebas dirigidas:** repetición desde directorios distintos, caché fría/caliente, antivirus/ruido documentado, orden aleatorio de casos y verificación de que instrumentar no cambia los artefactos.
- **Gate:** los cinco mayores consumidores de tiempo y memoria quedan identificados con evidencia; ninguna meta posterior se acepta sin comparación contra P00.

## NS2-PERF-P01 — Progreso, verbosidad y perfiles de tiempo / Progress, verbosity, and timing profiles

**Commit:** `nsc2-perf(p01): observable-compilation`

- **Implementación:** añadir salida normal con proyecto/fase/progreso y modos explícitos `--quiet`, `--verbose`, `--time-report` y `--time-trace=<archivo>`; separar progreso humano en `stderr` de salidas consumibles por máquinas.
- **Semántica:** el modo normal informa trabajo significativo sin inundar la consola; `--verbose` muestra entradas, decisiones de caché y comandos de subprocesos; `--time-report` resume tiempos; `--time-trace` conserva eventos jerárquicos analizables.
- **Pruebas dirigidas:** consola interactiva/redirigida, compilaciones paralelas, Unicode, rutas largas, cancelación, éxito y fallo temprano/tardío.
- **Gate:** ninguna compilación de más de dos segundos permanece silenciosa y cada fase principal puede atribuirse en el trace.

## NS2-PERF-P02 — Logs persistentes y diagnóstico de fallos / Persistent logs and failure diagnostics

**Commit:** `nsc2-perf(p02): persistent-diagnostics`

- **Implementación:** añadir `--log=<archivo>`, `--log-format=text|json`, `--log-on-error`, `--diagnostic-format=text|json` y `--save-failure-bundle=<directorio>`; centralizar diagnósticos estructurados con código estable, severidad, fase, span primario/secundario, notas, causas y sugerencias.
- **Contenido obligatorio:** versión/build de NSC, argumentos efectivos, proyecto, target, perfil, optimización, debug info, fases y tiempos, archivos relevantes, hits/misses de caché, comandos de LLVM/NSLink, stdout/stderr de subprocesos, cadena completa del error, código de salida y rutas de temporales conservados.
- **Seguridad:** redactar secretos y variables no autorizadas; imponer límites de tamaño; publicar el log o bundle de forma transaccional; un fallo al escribir el log no debe ocultar el error original.
- **Pruebas dirigidas:** error de parser, semántica, LLVM, PDB, NSLink, permisos, disco lleno simulado, proceso cancelado, dos builds concurrentes y JSON validado por schema.
- **Gate:** cualquier fallo del corpus oficial puede investigarse desde un único log o bundle sin buscar fragmentos en la consola.

## NS2-PERF-P03 — Startup, I/O y carga del proyecto / Startup, I/O, and project loading

**Commit:** `nsc2-perf(p03): reduce-fixed-build-cost`

- **Implementación:** eliminar descubrimiento y verificación redundantes de instalación/toolchain, lecturas repetidas de proyectos y biblioteca estándar, creación innecesaria de temporales, regeneración idéntica del runtime/import libraries y procesos externos evitables.
- **Pruebas dirigidas:** invocaciones consecutivas, múltiples proyectos en una solución, prefijos con espacios, instalación verificada/no verificada, metadata corrupta y modificación de una herramienta durante el build.
- **Gate:** el costo fijo se mide por separado y disminuye sin relajar la verificación de instalación ni reutilizar estado inválido.

## NS2-PERF-P04 — Frontend y análisis semántico / Frontend and semantic analysis

**Commit:** `nsc2-perf(p04): optimize-frontend`

- **Implementación:** perfilar y optimizar lexer/parser, interning, lookup, overload resolution, inferencia, instanciación genérica, ownership/lifetimes, construcción/verificación de MIR y recuperación de diagnósticos; eliminar copias, asignaciones y recorridos repetidos demostrados por perfiles.
- **Pruebas dirigidas:** archivos gigantes, overload sets amplios, genéricos profundos, errores patológicos, imports de alto fanout, código válido/inválido y comparación determinista de diagnósticos.
- **Gate:** mejora reproducible del frontend sin cambiar AST/MIR observable, resultado semántico ni catálogo de errores salvo corrección aprobada.

## NS2-PERF-P05 — Incrementalidad, fingerprints y caché / Incrementality, fingerprints, and cache

**Commit:** `nsc2-perf(p05): trustworthy-incremental-cache`

- **Implementación:** auditar y completar reutilización por archivo, módulo, API pública, instanciación genérica, MIR, COFF, PDB y producto enlazado; separar fingerprints source/ABI y registrar causas de hit, miss e invalidación.
- **Pruebas dirigidas:** no-change, cambio privado, cambio público, dependencia hoja/central, cambio de `-O`, `-g`, target o edición, caché corrupta, compilación interrumpida y dos procesos compitiendo por la misma entrada.
- **Gate:** un build sin cambios no repite frontend/codegen/enlace; una modificación invalida únicamente el cierre transitivo correcto; clean e incremental producen artefactos equivalentes.

## NS2-PERF-P06 — Paralelismo, memoria y cancelación / Parallelism, memory, and cancellation

**Commit:** `nsc2-perf(p06): bounded-parallel-builds`

- **Implementación:** formalizar el grafo de trabajos, `-j1`, `-jN` y selección automática; evitar oversubscription entre NSC/LLVM/NSLink; compartir datos readonly; limitar memoria por worker; cancelar trabajo inútil tras fallo fatal y ordenar diagnósticos determinísticamente.
- **Pruebas dirigidas:** 1/2/4/8/16 workers, poca memoria, muchos módulos pequeños, pocos módulos grandes, fallo concurrente, Ctrl+C, publicación simultánea y comparación byte a byte.
- **Gate:** escalar workers reduce tiempo en el corpus paralelo sin crecimiento de memoria no acotado, deadlocks, carreras ni variación de diagnósticos.

## NS2-PERF-P07 — Ruta rápida `-O0` / Fast `-O0` path

**Commit:** `nsc2-perf(p07): fast-o0`

- **Implementación:** definir `-O0` como la ruta de compilación más rápida compatible con código correcto y depuración fiel; eliminar passes de optimización, verificaciones duplicadas y transformaciones reservadas a producción cuando la evidencia permita hacerlo.
- **Auditoría obligatoria:** medir el costo de verificación LLVM por pass, partición de codegen, metadata `-g`, legalización SEH, runtime, PDB y enlace; una verificación costosa puede quedar en builds internos/CI sin gravar cada compilación normal, siempre que el gate de corrección permanezca cubierto.
- **Pruebas dirigidas:** `-O0 -g0`, `-O0 -gline-tables-only`, `-O0 -g`, SEH, closures, genéricos, inline obligatorio, stepping y equivalencia de comportamiento.
- **Gate:** O0 es el perfil de menor latencia y conserva todas las garantías semánticas; `--profile=debug` mantiene PDB y stepping verificables.

## NS2-PERF-P08 — Contratos reales `-O1`, `-O2` y `-O3` / Real `-O1`, `-O2`, and `-O3` contracts

**Commit:** `nsc2-perf(p08): optimization-level-contracts`

- **Implementación:** documentar y congelar objetivos, pipelines, límites de inlining/monomorfización y presupuestos de tiempo/tamaño para los cuatro niveles. Optimización y debug info siguen siendo selecciones independientes.
- **Contrato:** `O0` prioriza latencia/depuración; `O1` aplica optimizaciones baratas; `O2` es producción equilibrada; `O3` acepta mayor tiempo solo cuando demuestra mejoras relevantes.
- **Pruebas dirigidas:** tiempo de compilación, tiempo de ejecución, startup, memoria y tamaño sobre el mismo corpus; paridad semántica Debug/Release; perfiles con `-g0`, líneas y full debug.
- **Gate:** cada nivel ocupa una posición medible distinta. Si `O3` no mejora el corpus que lo justifica, no puede promocionarse como perfil útil.

## NS2-PERF-P09 — Calidad del código y costos ocultos / Generated-code quality and hidden costs

**Commit:** `nsc2-perf(p09): generated-code-performance`

- **Implementación:** instrumentar y reducir allocations, copies/moves, drops, bounds checks, dynamic dispatch, unwind edges, atomics/fences, closure overhead, code bloat genérico y costos de startup; inspeccionar MIR, LLVM IR y assembly.
- **Pruebas dirigidas:** microbenchmarks solo para aislar causas y programas completos para validar impacto; equivalentes en C/C++/Rust/Zig cuando la comparación sea justa y reproducible.
- **Gate:** O2 no regresa respecto al baseline en tiempo, memoria o tamaño sin waiver explícito; toda mejora conserva semántica y ABI; ningún benchmark puede sustituir programas testigo reales.

## NS2-PERF-P10 — NSLink, PDB, empaquetado y publicación / NSLink, PDB, packaging, and publication

**Commit:** `nsc2-perf(p10): toolchain-end-to-end-performance`

- **Implementación:** separar y optimizar tiempo de enlace, PDB, import libraries, runtime packaging, copias y publicación transaccional; definir integración de progreso/log/timing entre NSC y NSLink; medir full e incremental link.
- **Pruebas dirigidas:** EXE/DLL/LIB, O0–O3, `-g0`/líneas/`-g`, muchos objetos, response files, rutas largas, salida existente, publicación concurrente y fallo del linker.
- **Gate:** el usuario puede atribuir el tiempo end-to-end a NSC, backend, NSLink o packaging; el error del linker aparece completo en el log de NSC y conserva su código `NSLxxxx`.

## NS2-PERF-P11 — Certificación y promoción / Certification and promotion

**Commit:** `nsc2-perf(p11): certify-optimized-nsc-2`

- **Implementación:** ejecutar el corpus completo desde máquina limpia, publicar comparativa P00→P11, congelar contratos CLI/log/schema/perfiles y registrar waivers con propietario, causa y expiración.
- **Baseline mínimo:** 435/435 CTests, 148/148 fixtures y 18/18 casos de triage por enlazador como piso heredado de 2.0; ejecutar con NSLink y `lld-link`, O0/O1/O2/O3 según corresponda, clean/incremental, Debug/Release y determinismo desde dos directorios.
- **Gate:** cero regresiones semánticas o ABI no aprobadas; cero fallos conocidos que obliguen a buscar el error fuera del log; todas las metas de rendimiento congeladas en P00 están cumplidas o tienen waiver público y acotado.
- **Promoción:** solo después de este baseline puede abrirse `NS3-T00`.

## Integración con self-hosting 2.0

SH50/Tanda 52 es el gate normativo de rendimiento para la promoción autoalojada. Mide tres
repeticiones de las 80 unidades de biblioteca en O0–O3 con `nsc2`, exige determinismo por perfil,
cuatro contratos de objeto distintos y que O0 cumpla un presupuesto absoluto y relativo frente a
O2. La misma corrida certifica caché de artefactos, incrementalidad, perfiles, scheduling,
paralelismo, LTO y ThinLTO. Sus cifras y hashes quedan en
`docs/2.0/self-hosting/tandas/TANDA-52-SH50-RENDIMIENTO.md`; no sustituyen el programa detallado
P00–P11, sino que fijan su condición mínima de promoción 2.0.

SH51/Tanda 53 integra la superficie de observabilidad en el producto real. `--quiet` elimina salida
normal; `--verbose` activa progreso y tiempos; `--time-report` y `--time-trace=<archivo>` exponen el
perfil; `--log=<archivo>` y `--log-on-error=<archivo>` conservan argumentos, stdout, stderr y código
de salida en texto o JSON mediante publicación transaccional. El gate valida siete contratos CLI,
tres rutas de log, diagnósticos estructurados de `nsc2` y ocho pruebas de PDB/stepping/debugger.
Además, toda metadata producida se relee y su superficie pública y sus templates se analizan antes
de publicarse: NSC no puede volver a entregar una `.nsmeta` que él mismo rechace. La forma
`Module::Namespace.Type<T>.Member` es contrato común del escritor y del parser.

## Objetivos agregados iniciales / Initial aggregate targets

P00 debe convertirlos en metas por corpus y máquina de referencia. Como ambición inicial:

- reducir al menos 50% el tiempo clean de O0;
- reducir al menos 80% el tiempo de un build no-change;
- reducir al menos 30% el tiempo de O2 sin perder calidad de código;
- mantener o reducir memoria máxima y tamaño de artefactos salvo mejora justificada;
- mantener el overhead de logging/tracing por debajo de 5% cuando esté habilitado y despreciable cuando no;
- exigir que O3 demuestre mejoras en un corpus declarado para justificar su costo;
- conservar exactamente la semántica 2.0 y toda ABI publicada.

Estas metas no se rebajan silenciosamente. P00 puede recalibrarlas cuando una métrica absoluta resulte inválida, pero debe conservar la razón, la evidencia y la aprobación. Ninguna optimización se acepta porque “parece más rápida”: debe mostrar una mejora reproducible, conservar semántica y pasar el baseline completo.

