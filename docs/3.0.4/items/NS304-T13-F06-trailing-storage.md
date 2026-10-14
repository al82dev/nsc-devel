# NS304-T13-F06 — Trailing/append storage

- **Estado**: AUDITADO (layout consteval existe; trailing/append sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar trailing/append storage*.
Plan T13-F06 (DeepSeekDocs): *calcular layout consteval con overflow checks, alignment y bounds; el
objeto principal conoce longitud/capacidad contractual; vistas conservan provenance; ABI público
versionado*.

Estado real:

- El layout consteval de tipos (tamaño/alignment) ya existe (ObjectLayoutModel). El trailing/append
  storage (objeto con cola variable de longitud/capacidad contractual) no tiene surface.

## Gap

Falta el trailing/append storage: layout consteval con overflow/alignment/bounds + el objeto principal
con longitud/capacidad contractual + vistas con provenance + ABI versionado.

## Nota

F06 depende de F01 (allocator/provenance) + el layout consteval existente. No es una surface nueva; es
el layout de cola variable + el ABI.
