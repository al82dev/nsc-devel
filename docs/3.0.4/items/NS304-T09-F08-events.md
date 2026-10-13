# NS304-T09-F08 — Definir eventos nativos sobre handler/storage explícitos

- **Estado**: PARCIAL (handler explícito + add/remove + invoke) / residual (storage explícito custom)
- **Rama**: development

## Resultado

- **Parser** (`parseEventTail`): `event Handler Name;` declara el handler (delegate) explícito y
  baja un backing field `__nsc_event_<name>$stage42` (storage sintetizado) + add/remove. Además
  `event Handler Name { add { } remove { } }` baja los accessors custom como métodos
  `add_<name>`/`remove_<name>` con metadata `event:add:`/`event:remove:`.
- **Sema/HIR**: `p.Changed += Handle` (add), `Changed(42)` (invoke) resuelven y bajan a la storage.
- Pruebas: `TL-LANG-338` (add + invoke → 42) y `TL-LANG-341` (custom event parses/compiles) → **PASS**.

## Residual

- **Invocación de accessors custom**: `+=`/`-=` aún bajan a la storage sintetizada, no a los
  accessors `add`/`remove` custom (requiere cablear el flow de suscripción en Sema/HIR).
