# Tanda 07 — Errores y control de flujo / Errors and control flow

## Resultado / Outcome

ES: Se definieron `Result`/`?`, `throws E`, catches tipados, `never`, loops con valor y perfiles de
panic sin depender de CLR, RTTI administrado ni allocation implícita. EN: `Result`/`?`, typed
`throws`, typed catches, `never`, value loops, and panic profiles now have contracts independent of
CLR, managed RTTI, and implicit allocation.

## Archivos / Files

- Nuevos `ErrorSemantics.h/.cpp`: protocolo, ABI y reglas de flujo T07.
- Nueva `NativeSharp3T07ErrorTests.cpp`: nueve comprobaciones dirigidas.
- `CMakeLists.txt`: integra únicamente implementación y target T07.

## Mejora / Improvement

La propagación exige conversión explícita, los errores declarados deben manejarse o propagarse,
ningún unwind cruza FFI sin adapter, `never` participa en inferencia sin crear valores y el perfil
de panic decide de forma visible si hay cleanup. Propagation requires explicit conversion, declared
errors are handled or propagated, unwind cannot silently cross FFI, and panic cleanup is visible.

## Límite / Boundary

El modelo queda estable; parser y lowering branch/early-return/MIR se conectan conjuntamente en T15.
The model is stable; parser and branch/early-return/MIR lowering are integrated in T15.

## Validación / Validation

`nsc` O0 y ejecución directa de `nsc_ns3_t07_error_tests`; sin baseline ni CTest general.
