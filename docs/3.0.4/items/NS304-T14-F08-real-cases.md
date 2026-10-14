# NS304-T14-F08 — Casos reales

- **Estado**: AUDITADO (sin fixtures de derive tipado)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar casos reales de derive*.
Plan T14-F08 (DeepSeekDocs): *implementar fixtures de serializer, formatter, binding y trait derive;
generar código consumible sin runtime reflection; comparar manual y generado en ABI, metadata,
ejecución y costos visibles*.

Estado real:

- No hay fixtures de derive tipado (serializer/formatter/binding/trait) generando código consumible
  sin runtime reflection.

## Gap

Falta los cuatro derives (serializer/formatter/binding/trait) generando código consumible + la
comparación manual vs generado en ABI/metadata/ejecución/costos.

## Nota

F08 depende de F02–F07 (generación tipada + higiene + source mapping + integración + límites). Es el
cierre de T14 con fixtures reales, no una surface nueva.
