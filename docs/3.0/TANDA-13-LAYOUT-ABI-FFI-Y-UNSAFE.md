# Tanda 13 — Layout, ABI, FFI y unsafe / Layout, ABI, FFI, and unsafe

> **Reclasificación 2026-09-29:** `IMPLEMENTING`. Los asserts locales no sustituyen el
> layout/call classifier, hosts C ni ABI cross-module completo. La tanda operativa 19 conectó la
> metadata real a un reporte ABI estable y a un diff estructural. Véase
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

## Integración productiva 2026-09-29

La tanda operativa 19 añadió `nsc --dump-library-abi <file.nsmeta>` y
`nsc --abi-diff <old.nsmeta> <new.nsmeta>`. El reporte deriva de NSMETA verificada y hace visibles
layouts, bitfields, calling conventions, exports, dispatch y destrucción cross-module. El diff ABI
clasifica cambios físicos por identidad y diferencia una ruptura real de un cambio únicamente de
fuente. Evidencia y archivos: [TANDA-OPERATIVA-19-T13-ABI-REPORT-DIFF.md](TANDA-OPERATIVA-19-T13-ABI-REPORT-DIFF.md).

T13 no está cerrado: faltan unsafe productivo y auditable, punteros/provenance completos, SIMD
genérico, allocator provenance y lifetime/unload cross-module, hosts C/C++ amplios, plugin reject y
la matriz final con ambos linkers.
