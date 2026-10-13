# NS304-T07-F06 — Task scope, spawn/join y child failure

- **Estado**: AUDITADO (sin scope/spawn productivo; infraestructura histórica sin verificar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar task scope, spawn/join y child failure*.
Plan T07-F06 (DeepSeekDocs): *todo hijo pertenece a un scope y debe unirse o cancelarse antes de
salir; definir agregación/propagación de child failure, orden de cleanup y prohibición de borrows que
sobrevivan al scope; sin tareas huérfanas silenciosas*.

Estado real (tras F02):

- **Sin surface productiva**: no hay `TaskScope`, `spawn` ni `join` resueltos por el lenguaje.
- **Infraestructura histórica**: el plan menciona `NativeSharp3T10TaskScopeTests.cpp` y adapters de
  concurrencia/coroutines — existen como evidencia, no como feature cerrada (hay que verificar nombres
  actuales antes de editar).

## Gap

No hay scope estructurado que posea hijos, ni `spawn`/`join`, ni agregación de child failure. Un hijo
`spawn` no tiene dónde registrarse ni quién lo una/cancele.

## Decisión de surface (material, para Alberto)

- **(A) `TaskScope` explícito + `scope.Spawn(...)`/`scope.Join()`** (recomendada): el scope posee los
  hijos y exige join/cancel antes de salir.
- **(B) scoped `spawn { }` con join implícito al final del scope** (estilo structured concurrency).

Recomendación: **(A)**.

## Plan (tras surface)

1. Resolver `TaskScope`/`Spawn`/`Join` + registrar el hijo en el scope (rechazar huérfanos).
2. Agregar/propagar child failure (el fallo de un hijo finaliza el scope).
3. Orden de cleanup (hijos en orden inverso de spawn) + rechazo de borrows que sobrevivan al scope.

## Nota

F06 es un vertical grande (concurrencia estructurada) que depende de la surface de scope y de F05
(cancelación). No mezclar con paralelismo interno del compilador.
