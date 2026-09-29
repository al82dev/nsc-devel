# Tanda 16 — Compatibilidad y migración / Compatibility and migration

## Resultado / Outcome

ES: Se definió consumo de módulos 2.x, catálogo MIG3xxx, fix-its seguros, matriz source/ABI, guía
ejecutable y política de deprecación 3.x. EN: 2.x module consumption, MIG3xxx diagnostics, safe
fix-its, source/ABI classification, executable migration guidance, and 3.x deprecation are defined.

## Archivos / Files

- Nuevos `CompatibilitySemantics.h/.cpp`: clasificación y política T16.
- Nueva `NativeSharp3T16CompatibilityTests.cpp`: seis comprobaciones dirigidas.
- `CMakeLists.txt`: integra modelo y target T16.

## Mejora / Improvement

Un módulo viejo no se acepta por número solamente: estructura, target y huella ABI deciden direct,
adapter o reject. Los lints son estables, los cambios ambiguos jamás se editan automáticamente y
source/ABI se clasifican por separado. Old modules fail closed, migration edits are proof-based, and
consumer rebuild requirements derive from semantic/API facts.

## Límite / Boundary

La prueba dirigida valida reglas y formatos; el corpus histórico completo queda para el baseline
externo que Alberto ejecutará. Targeted validation does not run or replace the historical baseline.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t16_compatibility_tests`; sin baseline ni CTest general.
