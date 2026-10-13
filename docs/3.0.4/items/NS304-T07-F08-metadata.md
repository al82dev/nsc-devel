# NS304-T07-F08 — Metadata y debug

- **Estado**: AUDITADO (async-iterator opaco productivo; NSMETA/PDB/intermodular pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar metadata y debug del async iterator*.
Plan T07-F08 (DeepSeekDocs): *versionar descriptor de async iterator, operaciones, error, item, frame
opaco y constraints; emitir logical stacks y suspension points; probar consumo intermodular sin
fuentes y trazabilidad PDB*.

Estado real (tras F02):

- **Async-iterator opaco**: `async-iterator:<T>|<E>` resuelve + layout de 5 campos; el consumidor usa
  `MoveNextAsync`/`Current`/`Drop` por facade.
- **NSMETA**: sin descriptor del async iterator (no se publica en metadata el tipo/operaciones).
- **PDB**: sin suspension points/logical stacks del poll.
- **Intermodular**: sin consumo del async iterator cruzando módulos sin fuentes.

## Gap

Falta publicar el async iterator en NSMETA (tipo + operaciones + error + item + frame opaco), emitir
los suspension points en PDB, y consumirlo intermodularmente sin fuentes.

## Nota

F08 es el mismo cola de NSMETA/PDB/intermodular que T05-F09 y T06-F08. No es una surface nueva, sino
el plumbing de metadata/debug compartido por los tres opacos (future, iterator, async-iterator).
