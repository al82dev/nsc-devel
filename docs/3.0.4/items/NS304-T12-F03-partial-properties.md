# NS304-T12-F03 — Properties e indexers parciales

- **Estado**: PARCIAL (property parcial firma+accessor fusionados; indexers y conflictos get/set/init/ref pendientes)
- **Rama**: development

## Resultado (property parcial — 2026-10-14)

- **Parser** (`parsePropertyTail`): `partial` marca la property `partial-property` y cada accessor
  `partial-method`; el `get;`/`set;` parcial no sintetiza auto-storage ni marca abstract.
- **Member model**: dos `partial-property` con el mismo nombre fusionan (se conserva la primera);
  los accessors `get_`/`set_` parciales se fusionan por `signatureKey` como en F02 (body adjunto).
- Verificado: `partial int P { get; }` + `partial int P { get { return 5; } }` → `f.P` exit 5.
  Registrado `TL-LANG-417` (PASS). No se sintetiza storage/accessor faltante.

## Residual

- **Indexers parciales** y el **diagnóstico de get/set/init/ref conflict** (dos accessors del mismo
  tipo con body, o accessor incompatible entre partes) — el emparejamiento normativo completo.
