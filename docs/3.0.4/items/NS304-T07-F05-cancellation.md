# NS304-T07-F05 — Cancelación y cleanup bilateral

- **Estado**: CERRADO (cancelación cooperativa `it.Cancel()` + cleanup bilateral del `finally`); error del productor = T07-F03
- **Rama**: development

## Cierre — verificado 2026-10-14

- **Surface #5 FINAL** (`CancellationToken` + `it.Cancel()`): `it.Cancel()` marca el estado `-3` del
  async-iterator; el poll lo observa en el siguiente `yield` (`AsyncYieldValue`) y devuelve `false`
  (terminal). `TL-LANG-455` (exit 7).
- **Cleanup bilateral**: `yield return` captura las `cleanupRegionOrdinals` activas (el `finally`),
  `AsyncYieldValue` las propaga, y el bloque `ayield.cancelled` ejecuta el `finally`/drops una vez
  antes de aparcar el iterador. `TL-LANG-456` (exit 1).
- **Abandono**: el `Drop()` explícito + `DropFrame` (T06-F07/T07-F04) ya cubren el abandono brusco.
- Commits `2048bd8a` (cancelación), `52ce22f` (cleanup bilateral). Guards `386/387/400/445/452/454`
  y sync `383/384/391` PASS.

## Queda

- **Error del productor → future de demanda con error**: acoplado a T07-F03 (propagación runtime),
  no a F05.
- **`CancellationToken` como tipo explícito** (token observable pasable a hijos): el flag `-3` es la
  realización mínima del surface #5 sobre el async-iterator; el token nombrado + su propagación a
  `await` hijos es una ampliación posterior (T07-F07 Send/Sync + executor).
