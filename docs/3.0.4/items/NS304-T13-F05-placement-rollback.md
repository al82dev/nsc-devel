# NS304-T13-F05 — Placement/in-place y rollback

- **Estado**: AUDITADO (drop inverso existe; placement/rollback de construction sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar placement/in-place y rollback*.
Plan T13-F05 (DeepSeekDocs): *validar storage, alignment, capacidad y estado no inicializado; construir
bases y campos en orden; ante `throws E`/failure, destruir sólo subobjetos completos en inverso y
devolver storage a su estado contractual; no doble free/drop*.

Estado real:

- El drop inverso de subobjetos existe (destruction/cleanup, T09). La construcción placement/in-place
  con validación de storage/alignment/capacidad/estado + el rollback ante failure no tiene surface.

## Gap

Falta la construcción placement/in-place (validación storage/alignment/capacidad + construcción en
orden) + el rollback ante `throws E`/failure (destruir subobjetos completos en inverso, sin doble
free/drop).

## Nota

F05 depende de F01/F02 (allocator) + T09 (drop inverso + throws). No es una surface nueva; es el
placement + el rollback exacto de la construcción.
