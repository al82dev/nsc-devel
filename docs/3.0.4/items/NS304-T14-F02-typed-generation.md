# NS304-T14-F02 — Generación tipada

- **Estado**: AUDITADO (engines de metaprogramación base; builders AST validados + binding pendientes)
- **Rama**: development

## Auditoría (2026-10-14)

- `MacroExpansionEngine`/`ProjectMetaprogrammingModel` existen; el hueco son los **builders AST validados**
  (members/impls/helpers con origin + hygiene context + stable ID) + el paso por el **mismo binder** que
  fuente — la generación tipada que T12-F06 necesita. Vertical ancho, depende de F01.
