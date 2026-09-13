# Entrega 253 — Lectura, lexer y parser en paralelo / Parallel reading, lexing, and parsing

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 253.
- Resultado / Result: archivos independientes completan lectura, lexing y parsing mediante el pool
  acotado, conservando el mismo modelo observable que la ejecución de un solo worker.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

The syntax pipeline now has three explicit barriers. `syntax-read` reads every physical source into
a preassigned result slot. The coordinator then installs those buffers in canonical graph/source
order, which preserves historical `FileId` values. Generated precompiled buffers are appended by the
same coordinator. No worker mutates `SourceManager`.

`syntax-lex` starts only after every buffer and syntax unit exists. Each worker owns one unit and one
private `DiagnosticEngine`; the immutable `SourceManager` can therefore serve all lexer tasks safely.
El coordinador espera la barrera completa, determina la validez léxica y agrega los diagnósticos por
ordinal de unidad, nunca por orden de terminación.

After lexing, the coordinator derives the complete parser known-type set once. `syntax-parse` then
parses valid units independently into their own AST arenas. A final stable merge publishes parser
diagnostics. This preserves the existing global diagnostic budget while eliminating shared mutation
from lexer/parser workers.

`SourceManager::readFileBuffer()` contains the bounded 64 MiB and changed-file checks formerly
embedded in `loadFile()`. The serial API delegates to it, so both serial and parallel paths retain
one validation authority. `DiagnosticEngine::append()` similarly preserves range/location and
severity while merging a private task stream into the global stream.

## Archivos / Files

- `include/nativesharp/Source/SourceManager.h` and `src/Source/SourceManager.cpp`: read-only buffer
  acquisition separated from deterministic `FileId` publication.
- `include/nativesharp/Diagnostics/DiagnosticEngine.h` and
  `src/Diagnostics/DiagnosticEngine.cpp`: ordered diagnostic append operation.
- `src/Frontend/ProjectCompilation.cpp`: bounded read, lex and parse stages with explicit barriers.
- `tests/unit/ProjectCompilationTests.cpp`: serial/four-worker dump and token equivalence.
- `tests/unit/DiagnosticTests.cpp`: diagnostic merge count and order evidence.
- Compiler contract, conformance map and 2.0 roadmap: normative state and traceability.

## Decisiones / Decisions

1. Leer en workers y registrar en coordinador: `FileId` no depende de velocidad de disco.
2. Un arena AST por unidad: parser tasks do not share allocation state.
3. Diagnóstico privado por task: no mutex inside the lexer or parser hot path.
4. Barrera lexer/parser: known type discovery sees the complete deterministic token population.
5. Un solo pool por compilación sintáctica: threads are reused across all three stages.

## Aceptación / Acceptance

- Las lecturas físicas se envían como tareas independientes y conservan límites de entrada.
- Lexer y parser procesan unidades independientes usando el límite efectivo de `-j`.
- Uno y cuatro workers producen el mismo dump, tokens, ordinals y `FileId`.
- Los diagnósticos conservan rango, severidad, conteos y orden local durante el merge.
- Un fallo de lectura se selecciona por el primer source ordinal estable, no por el primero en acabar.
- Pruebas enfocadas y reconstrucción limpia de `nsc` pasan; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Paraleliza lectura lexer y parser / Parallelize reading lexer and parser`

**Descripción / Description**

`Run bounded per-file reads, lexers and parsers through the shared compiler worker pool, publish
buffers and syntax units in canonical order, isolate task diagnostics and merge them deterministically,
preserve FileIds and AST ownership, and prove exact serial/parallel token and dump equivalence.`
