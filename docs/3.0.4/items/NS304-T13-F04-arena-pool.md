# NS304-T13-F04 — Arena/pool contextual

- **Estado**: AUDITADO (ArenaAllocator es insumo; propagación del contexto a builders/containers pendiente)
- **Rama**: development

## Auditoría (2026-10-14)

- `ArenaAllocator` existe como insumo; el hueco es el **plumbing del contexto explícito** a builders/
  containers (quién destruye + cuándo se libera el bloque, sin global/thread-local oculto) + arenas
  anidadas + error parcial — un vertical ancho que depende de F01/F02.
