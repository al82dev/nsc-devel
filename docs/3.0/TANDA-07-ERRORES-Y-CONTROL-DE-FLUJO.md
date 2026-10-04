# Tanda 07 — Errores y control de flujo / Errors and control flow

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. El helper no implementa syntax, effects,
> MIR EH, funclets Win64, metadata ni debugger. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se definieron `Result`/`?`, `throws E`, catches tipados/filtrados, `never`, loops con valor y
perfiles de panic sin depender de CLR, RTTI administrado ni allocation implícita. `Result/?`,
`throws E` y `try/catch/finally` tipado ya tienen verticales productivas. EN: these features have
contracts independent of CLR, managed RTTI, and implicit allocation; `Result/?`, `throws E`, and
typed `try/catch/finally` now have productive verticals.

## Archivos / Files

- Nuevos `ErrorSemantics.h/.cpp`: protocolo, ABI y reglas de flujo T07.
- `NativeSharp3T07ErrorTests.cpp`: 28 comprobaciones dirigidas de Result, throws y typed catch.
- `CMakeLists.txt`: integra únicamente implementación y target T07.

## Mejora / Improvement

La propagación exige conversión explícita, los errores declarados deben manejarse o propagarse,
ningún unwind cruza FFI sin adapter, `never` participa en inferencia sin crear valores y el perfil
de panic decide de forma visible si hay cleanup. Propagation requires explicit conversion, declared
errors are handled or propagated, unwind cannot silently cross FFI, and panic cleanup is visible.

## Límite / Boundary

La tanda operativa 15 conectó una primera vertical física: `panic()` contextual de edición 3,
tipo interno `builtin:never`, nodo HIR/MIR explícito y perfil abort Win64 estable. El catch tipado
existente convive con esa vertical en c65. `Result/?` ya tiene una vertical productiva completa en
NS3-T07-F01. `throws E` ya es productivo mediante NS3-T07-F02: chequeo handle-or-propagate,
subtipado, delegates, HIR/MIR 1.24 y NSMETA 1.37. NS3-T07-F03 añade catches ordenados por tipo,
filtros `when`, cobertura de errores declarados, rethrow y finally/cleanup hasta MIR 1.25 y LLVM
Win64. Coerción general de `never`, loop expressions, constructores throwing, error sets y panic
unwind/handler permanecen abiertos.

Operational batch 15 connected the first physical vertical: edition-3 contextual `panic()`, the
internal `builtin:never` type, explicit HIR/MIR nodes, and a stable Win64 abort profile. Existing
typed catch coexists with that vertical in c65. `Result/?` is productive through NS3-T07-F01 and
typed `throws` through NS3-T07-F02. NS3-T07-F03 adds source-ordered typed catches, `when` filters,
declared-error coverage, rethrow, and finally/cleanup through MIR 1.25 and Win64 LLVM. General never
coercion, loop expressions, throwing constructors, error sets, and unwind/handler panic profiles
remain open.

## Validación / Validation

La evidencia acumulada incluye el cierre histórico de panic/never y las pruebas dirigidas de F01 y
F03. El target T07 terminó en `PASS: 28 checks` con O0/NSLink, incluido consumo NSMETA 1.37 entre
librerías, filtros nativos sin heap/CLR, nested try/rethrow/finally, orden de destrucción y ejecución
con salida 42. No se ejecutó baseline ni CTest general.
