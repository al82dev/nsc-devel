# NS304-T14-F07 — Límites y ciclos

- **Estado**: AUDITADO (presupuestos de generic/macro existen parcialmente; ciclos de expansión sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar límites y ciclos*.
Plan T14-F07 (DeepSeekDocs): *detectar recursion de expansión, ciclo de símbolos, colisión y
crecimiento sin límite; presupuestos de profundidad/nodos/memoria/pasos producen diagnóstico estable,
no crash/hang ni truncamiento silencioso*.

Estado real:

- `instantiationBudget_` (generic) existe; el macro/expansión no tiene presupuestos de profundidad/
  nodos/memoria/pasos + detección de recursion de expansión/cielo/colisión.

## Gap

Falta la detección de recursion de expansión + ciclo de símbolos + colisión + crecimiento sin límite +
los presupuestos de profundidad/nodos/memoria/pasos (diagnóstico estable, no crash/hang/truncamiento).

## Nota

F07 depende de F02 (generación) + el presupuesto genérico existente. No es una surface nueva; es la
seguridad de límites del macro.
