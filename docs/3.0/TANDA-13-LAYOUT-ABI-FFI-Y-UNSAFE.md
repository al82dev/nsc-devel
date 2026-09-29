# Tanda 13 — Layout, ABI, FFI y unsafe / Layout, ABI, FFI, and unsafe

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. Los asserts locales no sustituyen el
> layout/call classifier, metadata, hosts C ni ABI cross-module. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se fijaron representaciones, bitfields, garantías de puntero, calling conventions, catálogo
unsafe, contratos FFI y SIMD portable para Windows x64. EN: Representation, bitfields, pointer
guarantees, calling conventions, unsafe auditing, FFI contracts, and portable SIMD are now defined.

## Archivos / Files

- Nuevos `AbiSemantics.h/.cpp`: validación física y de fronteras T13.
- Nueva `NativeSharp3T13AbiTests.cpp`: ocho comprobaciones dirigidas.
- `CMakeLists.txt`: integra modelo y target ABI aislado.

## Mejora / Improvement

Combinaciones repr inválidas fallan antes de layout; los bitfields tienen máscara verificable; casts
que inventan garantías exigen unsafe; calling convention forma parte del contrato; buffers/callbacks
FFI tienen lifetime explícito; unwind no cruza la frontera; y SIMD conserva resultados al
escalarizar. Physical promises are explicit and auditable on the only supported target, Windows x64.

## Límite / Boundary

T13 congela reglas físicas. La emisión PE/COFF, NSMETA y verificación DLL conjunta se conecta en T15.
T13 freezes physical rules; PE/COFF, NSMETA, and DLL integration belongs to T15.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t13_abi_tests`; sin baseline ni CTest general.
