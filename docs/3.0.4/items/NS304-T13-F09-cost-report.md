# NS304-T13-F09 — Cost report

- **Estado**: AUDITADO (sin evidencia estructurada de storage/allocation/drop)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar cost report*.
Plan T13-F09 (DeepSeekDocs): *emitir evidencia estructurada de storage elegido, allocation count
esperado y drop/deallocation; pruebas demuestran stack/arena/heap explícito mediante allocator
instrumentado; no inferir costo sólo por ausencia de crash*.

Estado real:

- No hay reporte estructurado de storage elegido / allocation count / drop-deallocation; el `noheap`
  existente verifica ausencia de heap pero no emite el costo.

## Gap

Falta la evidencia estructurada del storage elegido + allocation count + drop/deallocation, y las
pruebas con allocator instrumentado (stack/arena/heap explícito) sin inferir por ausencia de crash.

## Nota

F09 es el cierre de T13 (evidencia de costo). Depende de F01 (provenance) + F08 (failure). No es una
surface nueva; es la instrumentación/reporte del allocator.
