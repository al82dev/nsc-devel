# NS304-T13-F02 — Selección del allocator en construcción

- **Estado**: AUDITADO (sitios de construcción tipados existen; selección de allocator sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar selección del allocator en construcción*.
Plan T13-F02 (DeepSeekDocs): *la sintaxis/binding elige allocator una vez y lo transporta al sitio
tipado de construcción; overload selection del constructor no cambia por accidente; diagnosticar
allocator incompatible o lifetime insuficiente*.

Estado real:

- `ProjectConstructionSite.*` + `ProjectConstructionModel.*` ya modelan sitios tipados de construcción
  (sin allocator).
- No hay surface de selección de allocator en `new`/construcción ni transporte al sitio.

## Gap

Falta la selección del allocator una vez en la sintaxis/binding + el transporte al sitio tipado de
construcción + el diagnóstico de allocator incompatible/lifetime insuficiente (sin alterar la overload
del constructor).

## Nota

F02 depende de F01 (protocolo allocator) + los sitios de construcción existentes. No es una surface
nueva; es el binding del allocator al sitio tipado.
