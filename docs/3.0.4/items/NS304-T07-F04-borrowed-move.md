# NS304-T07-F04 — Items borrowed y move-only

- **Estado**: CERRADO (ref/ref mut + move-only + liberación del frame async)
- **Rama**: development

## Cierre (ref/ref mut) — verificado 2026-10-14

- **Surface #2 FINAL** (`AsyncIterator<ref T,E>`/`AsyncIterator<ref mut T,E>`).
- **Causa del crash**: el HIR bajaba `yield return x` como `LocalValue` (el valor 10) en lugar de
  `LocalAddress(x)`, porque `YieldReturnStatement` sólo consultaba `iteratorInnerTypeKey` y no
  `asyncIteratorInnerTypeKey`. `item@16` guardaba 10 y `Current` lo desreferenciaba como puntero
  (0xA) → 0xC0000005.
- **Frame persistente**: el constructor (`Gen`) aloca el frame (heap) vía `GeneratorFrameInit`
  (post-pass copia el layout de `$moveNext$poll`); `AsyncYieldValue` publica `frame + offset`
  (sin fallback `alloca`; frame nulo = trap 0x7F04). `AsyncYieldValue` cuenta como suspensión en la
  liveness del `$poll`.
- **Borrow-check**: el ref-local de `Current` retiene `L<it>/F:Current` como place key;
  `MoveNextAsync`/`MoveNext`/`Drop` rechazan con borrow vivo; segundo `ref mut` del item rechazado.
- **Pruebas**: positivas `TL-LANG-445` (read 10), `446` (ref mut write 42), `447` (frame estable 120),
  `448` (await+yield 42); negativas `449` (avance con borrow), `450` (Drop con borrow), `451`
  (segundo ref mut). Commits `0c48f700` (crash+positivas), `60aa2bcb` (borrow-check+negativas).
- Guards `TL-LANG-386/387/400` y `TL-LANG-440/441/442` PASS.

## Cierre (move-only) — verificado 2026-10-14

- `yield return move(x)` permite el move (el yield es contexto consumidor); `move(it.Current)`
  transfiere el item una vez (`MoveIteratorCurrent`: carga `item@16` + zero del slot).
- Segundo `move(it.Current)` rechazado (`movedIteratorItems_` por place key del iterador).
- Drop tras move no doble-libera (slot zero). `TL-LANG-452` (exit 7), `453` (reject "twice").
  Commit `1ec8a5f9`.

## Cierre (liberación del frame async) — verificado 2026-10-14

- `DropFrame` libera el frame heap del async-iterator UNA vez + null del frame + estado terminal
  (`-1`); la segunda destrucción no re-libera (null-check). `TL-LANG-454` (double Drop exit 7).
  Commit `0ab8fa78`.
- El frame del **generator síncrono** no se libera aquí: su teardown de `foreach` conserva el
  doble-owner (`__foreach_source` + `__enumerator` comparten el frame por la copia de
  `GetEnumerator`), un defecto de T06-F08 que haría doble-free. Registrado como follow-up separado
  (commit causal, `HelpDeepSeek/NS304-T07-F04-frame-free.md`).
