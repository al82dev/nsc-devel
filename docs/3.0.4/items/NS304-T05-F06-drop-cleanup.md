# NS304-T05-F06 — Ejecutar drop/defer/finally una vez en completion, error o cancelación

- **Estado**: HECHO (drop en completion y tras suspensión, exactly-once; cancelación/error = F09/F07)
- **Rama**: development

## Resultado (drop tras suspensión — 2026-10-14)

- **Drop tras suspensión**: con F04 (liveness precisa) el owner sobrevive a la suspensión en el frame
  y su destructor corre **una vez** en el exit terminal (completion). Verificado O0 (`TL-LANG-435`,
  exit 142): `raii Box box; yield; return 42;` → `Drops=1`.
- **Completion** (previo): `rewriteAsyncPollReturns` copia `cleanupRegionOrdinals` al `return;`
  sintetizado (`TL-LANG-372`, exit 142).

## Residual (fuera de T05-F06)

- **Cancelación**: depende del ABI `drop` del future (F09). **Error**: depende de F07. Ambos son
  items posteriores del orden autorizado; el drop exactly-once compartido se cerrará allí.
