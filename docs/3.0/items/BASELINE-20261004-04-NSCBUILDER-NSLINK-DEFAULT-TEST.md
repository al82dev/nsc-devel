# Baseline 2026-10-04: expectativa NSLink de NSCBuilder

## Problema

NSCBuilder ya selecciona por defecto el artefacto Release normalizado en
`A:\nslink-devel\bin\release\nslink.exe`, pero su prueba de infraestructura todavía esperaba la
salida histórica `build-debug\bin\nslink.exe`.

## Cambio

- Se actualizó la expectativa del plan predeterminado para que coincida con la ruta productiva
  elegida por `nslink_executable("Release")`.

## Archivos

- `tests/selfhost/test_bootstrap_infrastructure.py`
- `docs/3.0/items/BASELINE-20261004-04-NSCBUILDER-NSLINK-DEFAULT-TEST.md`

## Mejora

La prueba vuelve a verificar el contrato vigente de NSCBuilder y deja de confundir una ruta de
compilación antigua con el artefacto seleccionado por el usuario.

## Validación dirigida

- Sólo `BootstrapInfrastructureTests.test_baseline_plan_defaults_to_nslink_without_lld_diagnostics`.
- No se ejecutó baseline ni la batería completa del archivo.
