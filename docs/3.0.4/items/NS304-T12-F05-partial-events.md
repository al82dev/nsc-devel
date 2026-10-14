# NS304-T12-F05 — Eventos parciales

- **Estado**: IMPLEMENTADO (evento parcial + add/remove fusionados; emparejamiento normativo = residual)
- **Rama**: development

## Resultado (firma + accessors — 2026-10-14)

- **Parser** (`parseEventTail`): `partial` marca el evento `partial-event` y cada accessor
  `partial-method`; el evento parcial con `add`/`remove` se acepta.
- **Member model**: dos `partial-event` (y su backing `event-backing:`) fusionan sin conflicto; los
  accessors `add_`/`remove_` parciales fusionan por `signatureKey` (F02).
- Verificado: `partial event Handler Changed;` + `partial event Handler Changed { add{…} remove{…} }`
  → add(+1) + remove(+2) = exit 3. Registrado `TL-LANG-419` (**PASS**). No hay multicast heap oculto.

## Residual (fuera del surface)

- **Emparejamiento normativo** de handler/accessibility/storage declarado + el diagnóstico de dos
  storage definitions o accessors incompatibles (hoy se fusiona por nombre/`signatureKey`).
