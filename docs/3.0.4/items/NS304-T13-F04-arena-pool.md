# NS304-T13-F04 — Arena/pool contextual

- **Estado**: AUDITADO (ArenaAllocator es insumo; contexto explícito sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar arena/pool contextual*.
Plan T13-F04 (DeepSeekDocs): *propagar contexto explícito a builders/containers que lo declaren; no
usar global/thread-local oculto; definir quién destruye elementos y cuándo se libera el bloque;
probar arenas anidadas y error parcial*.

Estado real:

- `ArenaAllocator` existe como insumo; no hay propagación de un contexto de arena a
  builders/containers (sin global/thread-local oculto).

## Gap

Falta propagar el contexto de arena/pool explícito a builders/containers (quién destruye elementos +
cuándo se libera el bloque) + arenas anidadas + error parcial.

## Nota

F04 depende de F01/F02 (allocator) + el contexto explícito. No es una surface nueva; es el plumbing
del contexto de arena a builders/containers.
