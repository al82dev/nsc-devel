# NS3-T16-F03 — Fix-its semánticamente seguros / Semantics-preserving fix-its

## Español

`nsc --apply-migration-fixes <entrada.ns> -o <salida.ns>` consume el mismo análisis MIG3xxx y
aplica únicamente ediciones que contienen una prueba explícita de equivalencia. La primera
transformación productiva convierte constantes decimales representables asignadas a `sbyte`,
`byte`, `short` o `ushort` en conversiones explícitas; valores fuera de rango, expresiones,
overloads, ownership, nullability, imports y cleanup permanecen como decisiones manuales.

Las ediciones se validan contra límites y solapamientos, se aplican de atrás hacia delante para
conservar offsets y requieren una ruta de salida distinta. La segunda ejecución no genera otra
edición: el formato es idempotente. Comentarios, literales y whitespace no se reconstruyen.

## English

`nsc --apply-migration-fixes <input.ns> -o <output.ns>` consumes the same MIG3xxx analysis and
applies only edits carrying an explicit equivalence proof. The first productive transformation
makes representable decimal constants assigned to `sbyte`, `byte`, `short`, or `ushort` explicit.
Out-of-range values, expressions, overloads, ownership, nullability, imports, and cleanup remain
manual decisions.

Edits are bounds/overlap checked, applied from the end to preserve offsets, and require a distinct
output path. A second pass produces no further edit. Comments, literals, whitespace, and unrelated
files are never reconstructed.

## Validación dirigida / Targeted validation

Construcción y ejecución directa de `nsc_ns3_t16_migration_tests`; sin CTest ni baseline.
