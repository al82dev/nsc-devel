# NS304-T14-F02 — Generación tipada

- **Estado**: AUDITADO (engines de metaprogramación base; builders AST validados sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar generación tipada*.
Plan T14-F02 (DeepSeekDocs): *permitir producir members, impls, helpers y declaraciones auxiliares
mediante builders AST validados; cada nodo lleva origin, hygiene context y stable ID; pasar por el
mismo binder que fuente; no insertar símbolos manuales laterales*.

Estado real:

- `MacroExpansionEngine`/`ProjectMetaprogrammingModel` existen como base; los builders AST validados
  (origin + hygiene context + stable ID) + el paso por el mismo binder no están cerrados.

## Gap

Falta los builders AST validados (members/impls/helpers/declaraciones) con origin/hygiene/stable ID y
el paso por el mismo binder que fuente (sin insertar símbolos manuales laterales).

## Nota

F02 depende de F01 (introspección) y es la generación tipada que T12-F06 necesita. No es una surface
nueva; es el cierre de los builders AST + el binding.
