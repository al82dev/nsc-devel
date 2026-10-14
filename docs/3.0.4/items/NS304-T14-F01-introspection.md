# NS304-T14-F01 — Introspección completa

- **Estado**: AUDITADO (Comptime/Macro/Metaprogramming son base; inventario de declaration kinds sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar introspección completa*.
Plan T14-F01 (DeepSeekDocs): *inventariar cada declaration kind vigente en 3.0.4 y exponer descriptors
tipados, identidad, modifiers, generics, effects, ownership y source origin; no devolver texto ni
omitir miembros nuevos silenciosamente; añadir gate que falle cuando aparezca un kind sin
visitor/introspection*.

Estado real:

- `ComptimeEngine`, `MacroExpansionEngine`, `ProjectMetaprogrammingModel`, `MetaprogrammingSemantics`
  existen como base (el self-host tiene `ComptimeProcessor.ns`).
- No hay inventario cerrado de declaration kinds + descriptors tipados (identidad/modifiers/generics/
  effects/ownership/source origin) + gate de kind sin introspection.

## Gap

Falta el inventario de declaration kinds + los descriptors tipados (no texto) + el gate que falla ante
un kind sin visitor/introspection.

## Nota

F01 es la base de T14 (antes de F02 generación tipada + F03 mixin). No es una surface nueva; es el
cierre de la introspección del AST vigente.
