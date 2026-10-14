# NS304-T13-F08 — Failure policy

- **Estado**: AUDITADO (null/Result/throws/panic existen por separado; perfil explícito sin integrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar failure policy*.
Plan T13-F08 (DeepSeekDocs): *integrar null/Result/`throws`/panic sólo según perfil explícito; `noheap`
rechaza cualquier ruta que intente fallback; cleanup de argumento, storage y subobjetos exacto en todas
las salidas*.

Estado real:

- null/Result/`throws`/panic existen por separado (T05/T09). No hay un **perfil explícito** que elija
  la política de failure del allocator/construcción, ni el rechazo de `noheap` ante fallback.

## Gap

Falta integrar la failure policy por perfil explícito (null/Result/`throws`/panic) + el rechazo de
`noheap` ante cualquier ruta de fallback + el cleanup exacto de argumento/storage/subobjetos.

## Nota

F08 depende de F01 (allocator) + T05/T09 (null/Result/throws). No es una surface nueva; es la
selección del perfil de failure + el `noheap` estricto.
