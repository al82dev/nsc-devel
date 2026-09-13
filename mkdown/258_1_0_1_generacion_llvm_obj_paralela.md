# Entrega 258 — Generación LLVM/OBJ paralela / Parallel LLVM and object generation

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 258.
- Resultado / Result: los productos EXE, LIB y DLL generan módulos LLVM y objetos COFF por unidad
  de código mediante el pool acotado y los enlazan en orden determinista.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`ProjectOopObjectGenerator` consumes the item-257 partition plan. With effective parallelism it
creates one coordinator module followed by one function module per code unit. Lowering tasks own
their LLVM context, module, builder, diagnostics and target emission state; no LLVM object crosses
worker mutation boundaries.

El módulo coordinador conserva una sola definición de todo el estado con identidad de programa:
statics mutables, cctors y sus guards, descriptores, vtables, mapas de interfaces, entradas de
destrucción, imports con binding dinámico, thunks de frontera y wrappers C. Las unidades de código
declaran ese estado como externo y sólo definen las funciones que les asignó la partición.

RTTI helpers and typed-catch filters remain internal module implementation details. Immutable
function-local literals may also be private per module. Callback boundary thunks are predeclared in
function units and defined once by the coordinator, preserving function-pointer identity across
all call sites.

Every module is verified, configured for Windows x64 and legalized/optimized before emission. LLVM
lowering and COFF emission are parallel. Optimization is deliberately an ordered loop in this
delivery so item 259 can parallelize that isolated phase without changing ownership or output
composition. Failures are consumed by module ordinal.

`ProjectOopObjectBundle` owns the temporary object directory for exactly as long as the linker needs
it. Its ordered object list always starts with the coordinator. EXE/DLL builders append one runtime
object and calculate imports from the concatenated LLVM text; the static builder archives all code
units plus one runtime-support object. Existing export, PDB and PE/COFF verification stays active.

The Driver now uses this path for linked project EXE, static library and DLL products and reports
`code-units`. One effective job retains the original monolithic object behavior. The explicit
single-OBJ command remains compatible because its public result is exactly one `.obj` file.

## Archivos / Files

- `include/nativesharp/LLVM/ProjectOopObjectGenerator.h` and
  `src/LLVM/ProjectOopObjectGenerator.cpp`: bundle lifetime, parallel module lowering, ordered
  optimization, parallel verified COFF emission and deterministic errors.
- `include/nativesharp/LLVM/ProjectOopLlvmLowerer.h` and
  `src/LLVM/ProjectOopLlvmLowerer.cpp`: partition-aware module ownership, external declarations and
  function-definition filters.
- `include/nativesharp/Linker/WindowsExecutableBuilder.h` and
  `src/Linker/WindowsExecutableBuilder.cpp`: multiobject executable link path.
- `include/nativesharp/Linker/WindowsLibraryBuilder.h` and
  `src/Linker/WindowsLibraryBuilder.cpp`: multiobject static/DLL link paths.
- `src/Driver/Driver.cpp`: job-aware generator integration for the three linked project products.
- `tests/unit/ProjectOopLoweringTests.cpp`: independent module verification, parallel object count
  and native DLL link coverage.
- `CMakeLists.txt`, compiler contract, conformance map and roadmap: build and traceability updates.

## Decisiones / Decisions

1. Coordinador + unidades de función: global identity has exactly one owner.
2. Contexto LLVM por tarea: LLVM builders and mutable modules are never shared concurrently.
3. Declaraciones completas por unidad: arbitrary calls across partitions remain ordinary symbols.
4. Orden coordinador primero: diagnostics, paths and linker input order are reproducible.
5. Bundle RAII: temporary COFF files survive linking and are removed afterward.
6. Fallback `-j1`: compatibility and small-project overhead remain controlled.
7. OBJ standalone sin cambio: a command promising one file continues producing one file.

## Aceptación / Acceptance

- Cada módulo particionado pasa el verificador LLVM independientemente.
- Cuatro workers emiten un objeto coordinador y un objeto verificado por unidad.
- Los objetos se enlazan en una DLL nativa sin símbolos duplicados ni faltantes.
- La prueba enfocada OOP lowering pasa 82 checks.
- Una invocación real `nsc -j 4 -O0 --emit-project-oop-exe` genera cuatro unidades, enlaza y el
  ejecutable termina con código 0.
- EXE, LIB and DLL Driver paths use the ordered multiobject builders.
- Reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Genera LLVM y objetos en paralelo / Generate LLVM and objects in parallel`

**Descripción / Description**

`Lower deterministic code partitions through isolated LLVM contexts, keep program-global identity
in one coordinator module, emit and verify COFF units in parallel, link ordered multiobject bundles
for EXE/LIB/DLL products, preserve the single-object compatibility path, and validate a real -j4
native executable.`
