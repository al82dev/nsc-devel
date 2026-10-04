# Tanda 07 — Errores y control de flujo / Errors and control flow

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. El helper no implementa syntax, effects,
> MIR EH, funclets Win64, metadata ni debugger. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se definieron `Result`/`?`, `throws E`, catches tipados/filtrados, `never`, loops con valor y
perfiles de panic sin depender de CLR, RTTI administrado ni allocation implícita. `Result/?`,
`throws E`, `try/catch/finally` tipado, la coerción general de `never` y los loops con valor ya tienen verticales
productivas. EN: these features have contracts independent of CLR, managed RTTI, and implicit
allocation; `Result/?`, `throws E`, typed `try/catch/finally`, general `never` coercion, and value loops now
have productive verticals.

## Archivos / Files

- Nuevos `ErrorSemantics.h/.cpp`: protocolo, ABI y reglas de flujo T07.
- `NativeSharp3T07ErrorTests.cpp`: 41 comprobaciones dirigidas de Result, throws, typed catch,
  `never` y loop expressions productivos.
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
filtros `when`, cobertura de errores declarados, rethrow y finally/cleanup hasta MIR 1.26 y LLVM
Win64. NS3-T07-F04 convierte `builtin:never` en el bottom type productivo, excluye caminos
divergentes de PHIs/stores y prueba loops infinitos sin fabricar layout. NS3-T07-F05 añade
`loop` como expresión, break/continue etiquetados, valores unificados, ownership/lifetimes y
cleanup hasta MIR 1.27 y LLVM Win64. Constructores throwing, error sets y panic unwind/handler
permanecen abiertos.

Operational batch 15 connected the first physical vertical: edition-3 contextual `panic()`, the
internal `builtin:never` type, explicit HIR/MIR nodes, and a stable Win64 abort profile. Existing
typed catch coexists with that vertical in c65. `Result/?` is productive through NS3-T07-F01 and
typed `throws` through NS3-T07-F02. NS3-T07-F03 adds source-ordered typed catches, `when` filters,
declared-error coverage, rethrow, and finally/cleanup through MIR 1.26 and Win64 LLVM. F04 makes
`builtin:never` the productive bottom type, excludes divergent paths from PHIs/stores, and proves
literal infinite loops without inventing a layout. F05 adds value-producing loops, labelled
break/continue, unified values, ownership/lifetimes, and cleanup through MIR 1.27 and Win64 LLVM.
Throwing constructors, error sets, and unwind/handler panic profiles remain open.

## Validación / Validation

La evidencia acumulada incluye el cierre histórico de panic/never y las pruebas dirigidas de F01 y
F03/F04/F05. El target T07 terminó en `PASS: 41 checks` con O0/NSLink, incluido consumo NSMETA 1.37 entre
librerías, filtros nativos sin heap/CLR, nested try/rethrow/finally, orden de destrucción y ejecución
con salida 42, coerción bottom, genéricos/overloads/match, loops demostrablemente infinitos y un
local `never` sin `alloca`. No se ejecutó baseline ni CTest general.
