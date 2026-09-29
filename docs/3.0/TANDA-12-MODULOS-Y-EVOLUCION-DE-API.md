# Tanda 12 — Módulos, nombres y evolución de API / Modules, names, and API evolution

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. El helper no gobierna graph, lookup,
> visibility, imports, NSMETA ni API diff productivos. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se fijaron identidad de módulo para Windows, ciclos resolubles, imports, visibilidad, reexports,
availability y una entrada canónica de huella API. EN: Windows module identity, resolvable cycles,
imports, visibility, reexports, availability, and canonical API fingerprint input are defined.

## Archivos / Files

- Nuevos `ModuleSemantics.h/.cpp`: contratos de módulo y superficie pública.
- Nueva `NativeSharp3T12ModuleTests.cpp`: ocho comprobaciones dirigidas.
- `CMakeLists.txt`: integra modelo y target T12.

## Mejora / Improvement

Un módulo no depende del casing/ruta física en Windows; ciclos de binding no se confunden con ciclos
de layout; imports son estables; una fachada no duplica identidad; y cambios privados/orden no
alteran la huella pública, mientras firma, efectos y layout sí. Module/API evolution is deterministic
and tooling can compare a semantic surface rather than source text.

## Límite / Boundary

T12 define el input canónico; T15 lo publica en NSMETA y conecta lookup incremental completo.
T12 defines canonical input; T15 publishes it through NSMETA and integrated lookup.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t12_module_tests`; sin baseline ni CTest general.
