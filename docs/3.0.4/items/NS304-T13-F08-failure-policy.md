# NS304-T13-F08 — Failure policy

- **Estado**: PARCIAL (noheap + rechazo de heap allocation operativos; perfil explícito de failure pendiente)
- **Rama**: development

## Resultado (noheap — 2026-10-14)

- **noheap**: el efecto `noheap` rechaza la construcción heap directa (`NS3030: noheap callable 'Make'
  cannot contain direct heap allocation 'object-construction'`) y el capture de closure que asigna
  entorno — ya operativo.

## Residual

- El **perfil explícito** que integra null/Result/`throws`/panic por construcción + el cleanup exacto
  de argumento/storage/subobjetos en todas las salidas.
