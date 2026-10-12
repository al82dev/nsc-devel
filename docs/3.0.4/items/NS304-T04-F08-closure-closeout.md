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

- Closures COMBINADAS con `errors` (closure que propaga/atrapa Error) — validar el carrier de
  error en el lambda (el `FunctionType.errorType` ya se arma en `functionType`/`bindLambdaToDelegate`).

## Plan

1. Confirmar con una prueba que un lambda con `throws Error` (closure + error) compila y ejecuta
   (layout + drop + escape + PDB combinados).
2. Prueba `TL-LANG-320`.

## Prueba puntual

`TL-LANG-320`: lambda con `throws Error` que captura un local y propaga; invocación captura el error.
