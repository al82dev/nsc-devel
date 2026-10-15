# NS304-T16-F08 — Manifest de superficie

- **Estado**: COMPLETO — `decimal` regenerado a `Implemented`; superficie sin `DeferredTo4` en el alcance.
- **Rama**: development

## Implementado (2026-10-15)

- `c1309955` — cierre del residual:
  - **Regeneración**: `Language3Feature::Decimal` pasa de `PlannedFor304` a `Implemented` con el
    rationale del ABI congelado (coeficiente 96 + escala + signo) + aritmética/overflow/conversión/
    formatting/FFI — reflejando el cierre de T15 (F01–F08) con sus pruebas.
  - **Validación de evidencia**: los tests `TL-LANG-155..160` (catálogo, lookup, enum externo,
    manifest, diagnóstico implementado/no disponible) se actualizaron y PASS; el conteo de
    `PlannedFor304` baja de 6 a 5 y `DeferredTo4` sigue en 0.

## Verificado O0

- `TL-LANG-155..160` (CL-LANG-012, Host.Language, `nsc_component_tests`): PASS — 16 entradas,
  `decimal=implemented`, 5 planes 3.0.4 + 1 rechazada, 0 diferidas.

## Contexto

- El hueco era la regeneración desde el inventario + la validación de que cada `implemented` apunta a
  pruebas/símbolos/metadata/self-host. El `decimal` (T15) era el residual que bloqueaba; ahora la
  superficie de 3.0.4 no conserva ningún `DeferredTo4` dentro del alcance. El resto del cierre de T16
  queda consolidado con T16-F01..F07.
