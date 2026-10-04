# NS3-T16-F05 — Guía de migración 2.x → 3.0 / Migration guide

## Español

`nsc --write-source-migration-guide <entrada.ns> -o <guia.md>` genera una guía específica para el
archivo analizado. No es una release note estática: agrupa hallazgos por capítulos MIG3xxx,
conserva su orden normativo, incluye cada fragmento before/after y marca la acción como
`automatic-safe` o `manual`.

La guía empieza con una estrategia incremental ejecutable: analizar, aplicar fix-its seguros,
resolver el inventario manual, compilar en edición 3/O0 y comparar NSMETA. Antes de escribirse se
valida que todo capítulo tenga identidad única, orden creciente, pares before/after y una prueba de
equivalencia para cada acción automática. Un archivo sin hallazgos recibe un capítulo `VERIFY`:
la ausencia de lints no sustituye la compilación ni las pruebas de comportamiento.

## English

`nsc --write-source-migration-guide <input.ns> -o <guide.md>` generates a guide for the analyzed
source file. It is not a static release note: findings are grouped into ordered MIG3xxx chapters,
each with before/after text and an `automatic-safe` or `manual` action.

The guide begins with an executable incremental strategy: analyze, apply safe fixes, resolve the
manual inventory, compile under edition 3/O0, and compare NSMETA. Before writing, the compiler
validates unique ordered chapters, before/after pairs, and an equivalence proof for every automatic
action. A clean source gets a `VERIFY` chapter because no lint never replaces compilation and
behavioral testing.

## Validación dirigida / Targeted validation

Construcción y ejecución directa de `nsc_ns3_t16_migration_tests`; sin CTest ni baseline.
