# Corrección 264D — Orden del timing / Pipeline timing order

## Origen / Origin

El baseline 1/16 dejó una sola expectativa roja en `nsc.driver`: el test de `--time` todavía
enumeraba la tubería anterior a la lectura paralela de fuentes. Esperaba `syntax-lex` antes de
`precompiled-syntax` y no incluía `syntax-read`.

La salida real seguía el pipeline correcto. Primero se carga metadata precompilada y el grafo;
después se leen todos los buffers físicos; luego se publican las unidades sintéticas procedentes de
NSMETA; sólo entonces el lexer ve el conjunto completo y el parser puede construir tipos conocidos.

## Implementación / Implementation

The Driver test now asserts the complete dependency order:

`precompiled-metadata -> project-graph -> syntax-read -> precompiled-syntax -> syntax-lex -> syntax-parse`

The remaining semantic HIR/MIR phases retain their previous order. The current compiler contract
now makes this sequence explicit and the conformance map points to the end-to-end timing assertion.

No se alteró el pipeline ni se ordenó artificialmente el reporte; se corrigió la especificación de
prueba para coincidir con las barreras implementadas y documentadas por la compilación paralela.

## Archivos / Files

- `tests/unit/DriverTests.cpp`: adds `syntax-read` and places precompiled syntax before lexing.
- Compiler contract and conformance map: publish the complete timing dependency order.

## Evidencia dirigida / Focused evidence

- The real Stage 30 timing report exposes all six front-end entry phases in the required order.
- `nsc_driver_tests` covers 213 driver checks.
- No additional baseline was run; this repairs the final known test root from baseline 1/16.

## Commit de referencia / Reference commit

**Título / Title**

`Actualiza el orden del perfil de compilación / Update compilation profiling order`

**Descripción / Description**

`Align the detailed timing contract with parallel source reading and precompiled syntax hydration,
assert every reached phase in dependency order, and retain deterministic total reporting without
changing the compiler pipeline.`
