# NS304-T16-F04 — EXE/DLL y ownership cross-module

- **Estado**: AUDITADO (export/import Win64 existen; ownership cross-module + unload sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar EXE/DLL y ownership cross-module*.
Plan T16-F04 (DeepSeekDocs): *cubrir exports/imports, callbacks, calling convention, allocation/
deallocation, borrow/move/drop y unload; instrumentar destrucción para detectar leak/doble drop;
prohibir que consumer dependa de static state o layout no publicado*.

Estado real:

- El export/import Win64 + runtime ABI existen; el ownership cross-module (allocation/deallocation,
  borrow/move/drop, unload) + la instrumentación de destrucción (leak/doble drop) no están cerrados.

## Gap

Falta cubrir allocation/deallocation, borrow/move/drop y unload cross-module + instrumentar la
destrucción (leak/doble drop) + prohibir static state/layout no publicado.

## Nota

F04 depende de F03 (producer/consumer) + el ABI Win64 existente. No es una surface nueva; es el
ownership + la destrucción cross-module.
