# Tanda 05 — Traits, interfaces estáticas y despacho / Traits and dispatch

## Resultado / Outcome

ES: Se fijaron contratos de traits sin layout, coherence global, tipos asociados, `some Trait`,
`dyn Trait`, extensiones y operadores. El despacho estático y dinámico conserva una identidad
distinta en metadata. EN: Contracts now cover layout-free traits, global coherence, associated
types, `some Trait`, `dyn Trait`, extensions, and operators. Metadata distinguishes static from
dynamic dispatch.

## Archivos y cambios / Files and changes

- Nuevos `TraitSemantics.h/.cpp`: validación y resolución determinista T05.
- Nueva `NativeSharp3T05TraitTests.cpp`: nueve comprobaciones puntuales.
- `CMakeLists.txt`: integra el modelo y registra su único ejecutable de prueba.

## Mejora / Improvement

ES: Un trait no altera layout; un impl huérfano o duplicado no puede cambiar semántica al importar
otro módulo; `some` evita boxing y `dyn` expone un fat pointer Win64 de 16 bytes. Los miembros reales
preceden extensiones y los operadores cortocircuitados no pueden sobrecargarse accidentalmente.

EN: Traits do not alter layout; orphan or duplicate impls cannot change meaning after an import;
`some` avoids boxing and `dyn` exposes an explicit 16-byte Win64 fat pointer. Real members precede
extensions and short-circuit operators cannot be accidentally overloaded.

## Límite / Boundary

El modelo semántico queda congelado aquí; parser, NSMETA y lowering end-to-end se conectan en T15.
The semantic model is frozen here; parser, NSMETA, and end-to-end lowering are integrated in T15.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t05_trait_tests` directo; sin baseline ni CTest general.
