# Tanda 04 — Genéricos 3.0 / Generics 3.0

> **Reclasificación 2026-09-29:** `DESIGNED/IMPLEMENTING`. El helper actual no está conectado al
> solver, monomorfización ni NSMETA productivos. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se añadió un núcleo determinista para constraints, const generics, packs, inferencia
bidireccional, aliases opacos, especialización y presupuestos de monomorfización. EN: A deterministic
core now covers constraints, const generics, packs, bidirectional inference, opaque aliases,
specialization, and monomorphization budgets.

## Cambios y archivos / Changes and files

- Nuevos `GenericSemantics.h/.cpp`: normalización estable, identidades y límites terminantes.
- Nueva `NativeSharp3T04GenericTests.cpp`: nueve verificaciones aisladas.
- `CMakeLists.txt`: enlaza el modelo y registra sólo el target específico T04.

## Mejora / Improvement

ES: La misma obligación produce la misma clave de caché; un valor const normalizado forma parte de
la identidad; los packs ambiguos, overlaps no ordenables y recursión infinita se rechazan con causa
estable. Un alias opaco conserva representación sin ser intercambiable fuera del módulo.

EN: Equivalent obligations produce the same cache key; normalized const values participate in
identity; ambiguous packs, unordered overlaps, and infinite recursion are rejected deterministically.
Opaque aliases retain their representation without becoming interchangeable outside their module.

## Límite / Boundary

ES/EN: T04 congela el solver contract. Parser, NSMETA y materialización completa se conectan en T15;
full parser, NSMETA, and lowering integration is intentionally centralized in T15.

## Validación / Validation

Compilación O0 de `nsc` y ejecución directa de `nsc_ns3_t04_generic_tests`; sin baseline general.
