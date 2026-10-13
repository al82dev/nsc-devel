# NS304-T04-F08 — Layout, drop, escape, PDB y closures con errors

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (línea base estática)

La infraestructura de closures es extensa y mayormente presente en código, pero su presencia no
equivale a evidencia de comportamiento:
- Layout: `computeClosureLayout` (CallableSemantics.cpp:246) + `closureLayoutSize/Alignment` en MIR.
- Drop: helper `__nsc_closure_context_destroy_` generado en LLVM (11772-11870), con destrucción de
  capturas `owned` y subprograma debug (11806).
- Escape: `ProjectReferenceEscapeEvaluator` + estadísticas `escapePropagationStatistics_`.
- PDB: subprogramas debug para el helper de destroy.

## Gap F08

- Las pruebas de ejecución no observan destrucción/drop de capturas, rechazo de escape ni el
  contenido PDB del closure y de su helper de destrucción.
- `TL-LANG-321` cubre ejecución de closure combinada con error; no certifica por sí sola las tres
  fronteras anteriores.

## Resultado parcial (validado)

- Ejecución de closure con captura: **`TL-LANG-320` PASS** (exit 42).
- Ejecución de closure combinada con error: **`TL-LANG-321` PASS**.
- Estos resultados no observan drop, escape ni PDB; tampoco convierten la auditoría estática de
  layout en una certificación de su contrato completo.

## Plan

1. Añadir un oráculo de destrucción/drop observable para capturas owned.
2. Añadir un negativo de escape con diagnóstico exacto.
3. Inspeccionar PDB de forma puntual y reproducible para closure y helper de destrucción.

## Prueba puntual

`TL-LANG-320` y `TL-LANG-321` validan ejecución. F08 permanece abierto hasta observar drop,
escape y PDB mediante oráculos específicos.
