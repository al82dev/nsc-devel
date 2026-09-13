# Entrega 254 — Índices y símbolos por archivo / Parallel per-file symbol indexing

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 254.
- Resultado / Result: predeclaraciones, namespaces, bindings de tipos y receptores de tipo se
  calculan por unidad en paralelo y se reducen a un modelo global estable.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

Name lookup is now split into two parallel map/reduce passes. The declaration pass walks each AST
without touching shared state and emits a `UnitDeclarationIndex`: namespace paths plus named-type
parts with their source identity. The coordinator consumes indexes by syntax-unit ordinal, mutates
module namespace trees, concatenates pending declarations and runs the existing canonical partial
type aggregation.

La segunda pasada comienza cuando todos los tipos y sus ordinals globales ya existen. Cada worker
resuelve una unidad completa contra tablas globales inmutables y produce un `UnitBindingIndex` con
bindings de `TypeSyntax`, receptores que nombran tipos y mapas locales por puntero AST. También usa
un motor de diagnósticos privado.

During reduction, local binding ordinals are rebased against the final vector offsets. Results and
diagnostics are appended in unit order. The physical order in which workers finish cannot affect
type ordinals, partial declarations, namespace contents, diagnostics or dump output.

The existing `ProjectNameLookup::build` entry point keeps a default of one worker for direct users;
`ProjectCompilation` supplies the effective compiler job count. Resolution algorithms and language
rules are unchanged: this commit changes ownership and scheduling of intermediate indexes only.

## Archivos / Files

- `include/nativesharp/Frontend/ProjectNameLookup.h`: per-unit declaration/binding result types and
  worker-count-aware build contract.
- `src/Frontend/ProjectNameLookup.cpp`: parallel map passes, deterministic reducers and ordinal
  rebasing.
- `src/Frontend/ProjectCompilation.cpp`: propagation of the effective job count into name lookup.
- `tests/unit/ProjectNameLookupTests.cpp`: complete serial/four-worker lookup dump equivalence.
- Compiler contract, conformance map and 2.0 roadmap: normative state and traceability.

## Decisiones / Decisions

1. Dos barreras: ningún binding se resuelve contra una tabla de declaraciones incompleta.
2. Namespaces privados por unidad: `GlobalNamespace` nunca recibe escrituras concurrentes.
3. Ordinals locales rebased: los workers no reservan posiciones globales ni usan atomics.
4. AST mutable sólo por dueño: alias expansion remains confined to the unit being bound.
5. Algoritmos de resolución compartidos: no se duplicó ni simplificó semántica para paralelizar.

## Aceptación / Acceptance

- Declaraciones y namespaces de archivos independientes se indexan mediante el pool acotado.
- Type bindings y expression-type bindings se producen en una segunda pasada paralela.
- Partial types, nested types, aliases, accessibility and transitive visibility remain covered.
- Uno y cuatro workers generan exactamente el mismo `ProjectNameLookup` dump.
- La prueba enfocada completa de name lookup pasa.
- Reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Indexa símbolos por archivo en paralelo / Index symbols per file in parallel`

**Descripción / Description**

`Build private per-unit declaration and name-binding indexes through the compiler worker pool,
merge namespaces and type parts before canonical aggregation, rebase local binding ordinals during
stable reduction, preserve language resolution semantics, and prove exact serial/parallel lookup
dump equivalence.`
