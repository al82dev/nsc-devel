# NS304-T16-F07 — Lints, fix-its y migración

- **Estado**: COMPLETO — reglas de migración 2.x→3.0.4 documentadas por cambios T03–T15 + fix-it
  idempotente + guía ejecutable verificados.
- **Rama**: development

## Reglas por cambios reales (T03–T15)

- Los cambios T03–T15 son **features nuevas** (decimal, Result/throws/panic, arena/allocator,
  macros/mixin, trait/extension blocks, layout) — no reemplazan un comportamiento 2.x, por lo que no
  introducen un `MigrationIssue` nuevo; las 7 reglas existentes (`MIG3001..MIG3007`:
  NewlyContextualWord/OverloadResolution/CopyToMove/Nullability/ImportLookup/NumericNarrowing/
  CleanupOrder) cubren el corpus 2.x→3.0 y se conservan intactas.
- **Fix-it idempotente**: `applySafeMigrationFixes` aplica sólo ediciones probadas, no solapadas y en
  orden; re-analizar el resultado no deja otro `safeFix` (idempotencia verificada).
- **Guía ejecutable**: `ExecutableMigrationGuide` con capítulos ordenados, "Before:"/"After:",
  acciones `automatic-safe` e inventario `Manual-decision`.

## Verificado O0

- Unit `nsc_ns3_t16_migration_tests`: PASS (27 checks) — analizador productivo, ediciones idempotentes,
  guía con antes/después, matriz de compatibilidad, política de deprecación y modo estricto.
- Component `DriverMigration345To432Module` (`nsc_component_tests`): PASS — lexical/fix/guide/baseline.

## Contexto

- El hueco era la actualización de reglas + el fix-it idempotente + la guía ejecutable (doc + lints
  existentes). El fix-it idempotente y la guía ejecutable ya operaban y se verificaron; la
  actualización de reglas quedó documentada arriba: los cambios T03–T15 son aditivos y no demandan un
  issue nuevo.
