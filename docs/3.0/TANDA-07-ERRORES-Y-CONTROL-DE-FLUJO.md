# Tanda 07 — Errores y control de flujo / Errors and control flow

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. El helper no implementa syntax, effects,
> MIR EH, funclets Win64, metadata ni debugger. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

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

La tanda operativa 15 conectó una primera vertical física: `panic()` contextual de edición 3,
tipo interno `builtin:never`, nodo HIR/MIR explícito y perfil abort Win64 estable. El catch tipado
existente convive con esa vertical en c65. `Result/?`, typed `throws`, coerción general de `never`,
loop expressions y panic unwind/handler permanecen abiertos; el helper por sí solo no los cierra.

Operational batch 15 connected the first physical vertical: edition-3 contextual `panic()`, the
internal `builtin:never` type, explicit HIR/MIR nodes, and a stable Win64 abort profile. Existing
typed catch coexists with that vertical in c65. `Result/?`, typed `throws`, general never coercion,
loop expressions, and unwind/handler panic profiles remain open; the helper does not close them.

## Validación / Validation

`nsc` O0, c65 con lld-link y NSLink, probe abort con salida 142, MIR 1.18 y dos negativos
dirigidos; sin baseline ni CTest general.
