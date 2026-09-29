# Tanda 03 — Tipos suma y pattern matching

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. El módulo actual es un prototipo C++
> aislado; no constituye una feature integrada de NativeSharp. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se estableció el contrato cerrado de `variant` y patrones para NativeSharp 3.0: tag mínimo,
payload in-place, layout Windows x64, bindings con modo de ownership, exhaustividad, brazos
inalcanzables, guards, `let else`, uniones C y una identidad determinista para metadata.

EN: This batch establishes NativeSharp 3.0's closed `variant` and pattern contract: minimum tag,
in-place payload, Windows x64 layout, ownership-aware bindings, exhaustiveness, unreachable arms,
guards, `let else`, C unions, and deterministic metadata identity.

## Archivos / Files

- Nuevo `include/nativesharp/Language/SumTypeSemantics.h`: tipos públicos del contrato T03.
- Nuevo `src/Language/SumTypeSemantics.cpp`: layout y análisis determinista sin allocation runtime.
- Nuevo `tests/unit/NativeSharp3T03SumTypeTests.cpp`: ocho verificaciones dirigidas.
- Modificado `CMakeLists.txt`: incorpora el modelo y únicamente su target de prueba.

## Mejora / Improvement

ES: Un variant representa sólo estados declarados, destruye conceptualmente un único payload activo
y puede publicar su catálogo de casos sin source. Los guards no falsean exhaustividad y una `union`
C sigue siendo un contrato distinto, cuyo acceso inseguro queda explícito.

EN: A variant represents declared states only, conceptually destroys one active payload, and can
publish its case catalogue without source. Guards cannot fake exhaustiveness, and a C `union`
remains a separate contract whose unsafe access is explicit.

## Límite e integración / Boundary and integration

ES: Esta tanda congela el modelo semántico comprobable. El parser completo de patterns y su
materialización OOP/MIR/LLVM se conectan en T15 para evitar afirmar codegen antes del pipeline
integrado. EN: This batch freezes the testable semantic model. Full pattern parsing and OOP/MIR/LLVM
materialization are connected in T15; no premature end-to-end codegen claim is made.

## Validación dirigida / Targeted validation

`nsc` en O0 y ejecución directa de `nsc_ns3_t03_sum_type_tests`; sin baseline ni CTest general.
