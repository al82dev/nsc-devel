# NS3-T16-F06 — Ventana de deprecación / Deprecation window

## Español

`nsc --print-deprecation-policy` publica `NSDEPRECATION 1`: siete aliases/comportamientos 2.x,
su código MIG3xxx, release de deprecación y release mínima de eliminación. En edición 2 siguen
activos; en edición 3 producen warning, admiten la supresión localizada del analizador y se vuelven
error con `nsc --analyze-migration-strict <archivo.ns>` para CI. La política actual fija 4.0 como
primera eliminación ordinaria.

`nsc --simulate-deprecation-removal <feature> <major.minor> [--security-ub]` permite auditar el
calendario. Una eliminación 3.x se rechaza aunque haya warning previo; sólo una corrección de
seguridad/UB marcada explícitamente puede adelantarla. Una eliminación ordinaria desde 4.0 exige
que el feature haya pasado por deprecación.

## English

`nsc --print-deprecation-policy` publishes `NSDEPRECATION 1`: seven 2.x aliases/behaviors, their
MIG3xxx code, deprecation release, and earliest removal release. They remain active in edition 2;
edition 3 emits warnings, honors the analyzer's local suppression, and turns warnings into CI
errors through `nsc --analyze-migration-strict <file.ns>`. The current ordinary-removal floor is
4.0.

`nsc --simulate-deprecation-removal <feature> <major.minor> [--security-ub]` audits the calendar.
Ordinary 3.x removal is rejected even after a warning; only an explicitly documented security/UB
fix may advance it. Ordinary removal from 4.0 onward requires prior deprecation.

## Validación dirigida / Targeted validation

Construcción y ejecución directa de `nsc_ns3_t16_migration_tests` y del helper T16 existente;
sin CTest ni baseline.
