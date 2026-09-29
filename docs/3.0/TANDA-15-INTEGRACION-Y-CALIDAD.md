# Tanda 15 — Integración del lenguaje y calidad / Language integration and quality

> **Reclasificación 2026-09-29:** `DESIGNED`. La prueba actual compone helpers C++; no compila un
> corpus NativeSharp ni valida artefactos, tooling o costos reales. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se añadió un gate integrado para matriz de interacciones, corpus testigo, presupuestos, calidad
diagnóstica, nombres semánticos y costos generados; además cruza directamente los contratos T03–T14.
EN: An integrated gate now covers interaction matrices, witness programs, budgets, diagnostic
quality, semantic names, generated costs, and direct composition of the T03–T14 contracts.

## Archivos / Files

- Nuevos `IntegrationQuality.h/.cpp`: contratos transversales y gate de costos/calidad.
- Nueva `NativeSharp3T15IntegrationTests.cpp`: once comprobaciones de interacción y composición.
- `CMakeLists.txt`: integra el gate y su único target dirigido.

## Mejora / Improvement

Las features dejan de evaluarse como islas: ownership cruza await, Result conserva layout variant,
traits genéricos preservan dispatch, repr FFI entra a fingerprint y consteval depende de inputs
declarados. Costos de allocation/copy/drop/dyn/unwind/fence deben aparecer como explícitos.

Features are no longer evaluated in isolation, and tooling consumes stable semantic names/schema.

## Alcance honesto / Honest scope

Esta prueba dirigida valida la integración de contratos implementada durante las tandas sin ejecutar
el baseline prohibido. El baseline externo final sigue siendo la autoridad para corpus completo,
PE/COFF/PDB, DLL y regresiones 2.x. This targeted test does not replace the final external baseline.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t15_integration_tests`; sin baseline ni CTest general.
