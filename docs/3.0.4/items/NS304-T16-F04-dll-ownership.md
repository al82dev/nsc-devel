# NS304-T16-F04 — EXE/DLL y ownership cross-module

- **Estado**: AUDITADO (export/import Win64 + runtime ABI existen; ownership cross-module + unload pendientes)
- **Rama**: development

## Auditoría (2026-10-14)

- El hueco es el **corpus cross-module**: allocation/deallocation, borrow/move/drop y unload +
  instrumentación de destrucción (leak/doble drop) + prohibir static state/layout no publicado — pruebas
  intermodulares sobre el ABI Win64 existente, no una feature de lenguaje.
