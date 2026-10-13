# NS304-T09-F08 — Definir eventos nativos sobre handler/storage explícitos

- **Estado**: PARCIAL (handler explícito + add/remove + invoke) / residual (storage explícito custom)
- **Rama**: development

## Resultado

- **Parser** (`parseEventTail`): `event Handler Name;` declara el handler (delegate) explícito y
  baja un backing field `__nsc_event_<name>$stage42` (storage sintetizado) + add/remove.
- **Sema/HIR**: `p.Changed += Handle` (add), `Changed(42)` (invoke) resuelven y bajan a la storage.
- Prueba `TL-LANG-338`: add handler + invoke → `Result` = 42 → **PASS**.

## Residual

- **Storage explícito custom** (event con accessors `add`/`remove` sobre storage declarado por el
  usuario): hoy la storage es siempre sintetizada.
