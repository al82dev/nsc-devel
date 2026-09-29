# Tanda 08 — Funciones, lambdas y closures / Functions, lambdas, and closures

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. La representación contractual no está
> conectada con tipos función, capture analysis, lowering, metadata o debugger. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se fijó identidad completa de tipos función, conversión de lambdas sin captura, environments
nativos de closure, capturas copy/move/ref, traits callable, lambdas genéricas, métodos bound y
`must_tail`. EN: Complete function identity, non-capturing conversion, native closure environments,
copy/move/ref captures, callable traits, generic lambdas, bound methods, and `must_tail` are defined.

## Archivos / Files

- Nuevos `CallableSemantics.h/.cpp`: contratos de representación, ownership y ABI.
- Nueva `NativeSharp3T08CallableTests.cpp`: nueve comprobaciones específicas.
- `CMakeLists.txt`: incorpora modelo y target T08 aislado.

## Mejora / Improvement

Una función como valor ya no pierde calling convention, safety, nullability o efecto throws; una
closure es un struct nativo de costo visible; las capturas move producen `FnOnce`; los borrows que
escapan se rechazan; y una tail call obligatoria falla si ABI o cleanup la hacen imposible.

Function values retain ABI/effects, closures have visible native layout, move capture implies
`FnOnce`, escaping borrows are rejected, and required tail calls are verified rather than hinted.

## Límite / Boundary

T08 congela identidad y validación. Parser, NSMETA, monomorfización y lowering integrado se conectan
en T15. T15 owns full parser, metadata, monomorphization, and lowering integration.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t08_callable_tests`; sin baseline ni CTest general.
