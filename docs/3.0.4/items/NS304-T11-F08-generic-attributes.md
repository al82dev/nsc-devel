# NS304-T11-F08 — Atributos genéricos

- **Estado**: AUDITADO (atributos nominales existen; atributo genérico sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar atributos genéricos*.
Plan T11-F08 (DeepSeekDocs): *aceptar tipo de atributo genérico y argumentos consteval válidos; cerrar
instanciación, identidad y serialización NSMETA; rechazar argumento runtime, tipo ref-like donde
metadata no lo represente y duplicados incompatibles*.

Estado real:

- Atributos nominales existen (parser/sema/metadata). Un tipo de atributo **genérico** no se acepta.
- Argumentos de atributo consteval: sin validación de consteval/runtime; serialización NSMETA de
  genéricos sin cerrar.

## Gap

Falta aceptar el tipo de atributo genérico + argumentos consteval válidos, cerrar instanciación/
identidad/serialización NSMETA, y rechazar argumentos runtime, ref-like no representable y duplicados
incompatibles.

## Nota

F08 depende de F05 (ref-like) + la instanciación genérica existente. No es una surface nueva; es el
cerrar de los atributos sobre genéricos + metadata.
