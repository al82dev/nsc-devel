# Tanda 14 — Semántica numérica y constantes / Numeric and constant semantics

## Resultado / Outcome

ES: Se fijaron literales tipados, overflow checked/wrapping/saturating/unchecked, conversiones sin
pérdida, modos FP, newtypes numéricos y paridad consteval/runtime. EN: Typed literals, explicit
overflow modes, lossless conversions, FP modes, numeric newtypes, and consteval/runtime parity are set.

## Archivos / Files

- Nuevos `NumericSemantics.h/.cpp`: semántica numérica única e independiente de optimización.
- Nueva `NativeSharp3T14NumericTests.cpp`: siete comprobaciones dirigidas.
- `CMakeLists.txt`: integra modelo y target T14.

## Mejora / Improvement

Debug y Release ya no pueden escoger políticas distintas; overflow de literal ocurre antes del cast;
las conversiones implícitas no pierden información; los modos FP publican sus libertades; y un
newtype conserva identidad sin wrapper físico. Debug/Release behavior is no longer accidental and
compile-time arithmetic calls the same evaluator as runtime semantics.

## Límite / Boundary

T14 congela reglas; T15 conecta diagnósticos, MIR y selección de flags LLVM sobre estas decisiones.
T14 freezes rules; T15 integrates diagnostics, MIR, and LLVM flag selection.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t14_numeric_tests`; sin baseline ni CTest general.
