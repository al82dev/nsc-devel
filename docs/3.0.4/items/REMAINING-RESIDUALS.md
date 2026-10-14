# Parte B 3.0.4 — residuales pendientes (estado de sesión)

Resumen honesto de lo que queda. Los ítems cerrados/avanzados están en el
`NATIVESHARP-3.0.3-A-3.0.6-TRACKING-MAESTRO.md` (marcas `[x]`). Cerrados hasta ahora: T00-T04
(38/147) + T05-F01/F02/F03/F10 + T06-F01/F02/F03 + T07-F01/F02 + T08-F01..F04 + T09-F01/F02/F03/
F05/F07/F08 + T10-F02/F04/F07 = **60/147 (40.82 %)**.

## Residuos acotados (multi-capa media)

- **T08-F07 (NSMETA static/operator)** — la extensión estática y el operador marcan el flag
  `kMetadataNamespaceFunctionExtension` (NSMETA 1.32) sin un bit "static"; el consumidor los
  reconstruye como extensión de instancia y descarta el primer parámetro (operador intermodular
  roto). Fix acotado: bit "static" en `LibraryMetadataNamespaceFunction::flags` o no marcar
  `isExtension` en `ns3-extension-static` + prueba de consumo intermodular.
- **T07-F03** — backpressure/terminalidad/orden observables (el error tipado `<E>` ya es productivo).

## Bloques duros (multi-capa grande)

- **T05-F04..F09 / T06-F04..F08 / T07-F04..F08** — liveness precisa, pinning, drop en error/
  cancelación, backpressure, task scope, Send/Sync, metadata/debug de async frames.
- **T03-F03 (Span/ReadOnlySpan)** — bloqueado: `ensureTypeSpecialization` no materializa el tipo
  desde cuerpos de método.
- **T09-F06** — length const genérico (`const N`) — vertical de type-system.
- **T09-F09/F10** — `throws E` constructors, cleanup parcial, ABI/NSMETA/unwind.
- **T10-F01** — literales UTF-8 tipados (`u8"..."`).
- **T10-F03** — interpolation handlers productivos.
- **T10-F05** — indexador de usuario (la base `?[]` nativo ya está).
- **T11-T17** — spans/ref-like, partial members, allocators, comptime, decimal, ABI/metadata/debug,
  conformidad C# 14/Beef y cierre (suite O0 final reservada a Alberto, T17-F08).

## Nota

Las 12 decisiones de surface están fijadas en `NS304-DECISIONES-SURFACE.md` (no bloquean). Los
commits aislados por ítem y las pruebas puntuales (`TL-LANG-*`, `TP-PAR-*`, `TL-LEX-*`, `TA-AST-*`)
se mantienen verdes.
