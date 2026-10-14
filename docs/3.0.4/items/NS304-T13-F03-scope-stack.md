# NS304-T13-F03 — Scope/stack

- **Estado**: AUDITADO (stack automático + drop existen; escape de scope sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar scope/stack*.
Plan T13-F03 (DeepSeekDocs): *construir en storage automático con lifetime léxico y drop automático;
escape analysis rechaza retorno, capture o almacenamiento más largo; tamaño/alignment consteval o
contrato explícito, nunca heap fallback*.

Estado real:

- La construcción en stack + drop automático (RAII) ya existe (destruction/cleanup). El storage
  automático con lifetime léxico no tiene una surface de `scope` que rechace el escape (retorno/
  capture/almacenamiento más largo).

## Gap

Falta la surface `scope`/stack (construcción en storage automático con lifetime léxico) + el escape
analysis que rechaza retorno/capture/almacenamiento más largo + tamaño/alignment consteval (nunca
heap fallback).

## Nota

F03 depende de F01/F02 (allocator) + el drop RAII existente. No es una surface nueva de drop; es el
lifetime léxico + el escape analysis.
