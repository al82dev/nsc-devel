# NS304-T04-F08 — Layout, drop, escape, PDB y closures con errors

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (línea base)

La infraestructura de closures es extensa y mayormente presente:
- Layout: `computeClosureLayout` (CallableSemantics.cpp:246) + `closureLayoutSize/Alignment` en MIR.
- Drop: helper `__nsc_closure_context_destroy_` generado en LLVM (11772-11870), con destrucción de
  capturas `owned` y subprograma debug (11806).
- Escape: `ProjectReferenceEscapeEvaluator` + estadísticas `escapePropagationStatistics_`.
- PDB: subprogramas debug para el helper de destroy.

## Gap F08 (a confirmar)

- Closures COMBINADAS con `errors`: la validación necesita la core library (el tipo `Error` es el
  root de excepciones, definido en el subset de core, como TL-LANG-310). La prueba `TL-LANG-321`
  (closure + `throws Error`) se redactará copiando el subset de core.

## Resultado parcial (validado)

- Layout + drop + ejecución de closure con captura: **`TL-LANG-320` PASS** (exit 42).

## Plan

1. Prueba `TL-LANG-321`: closure sobre delegate `throws Error` con subset de core copiado
   (Object.ns + error root), invocación en try/catch.
2. Confirmar layout + drop + escape + PDB combinados con error.

## Prueba puntual

`TL-LANG-320` (closure layout+drop) ✅; `TL-LANG-321` (closure + throws Error) pendiente con core subset.
