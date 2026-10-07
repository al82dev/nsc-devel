# Tanda universal 12: evidencia determinista de COM vtable

## Objetivo

Preservar el diagnóstico producido por la prueba `nsc.item339.com-vtable-production`
antes de clasificar su fallo como compilación, enlace, ABI o ejecución.

## Causa confirmada

La prueba ejecutaba `compileAndRun(...)` y construía el mensaje con `diagnostic` en
argumentos distintos del mismo llamado a `expect`. El orden de evaluación de esos
argumentos no garantiza que el diagnóstico se haya llenado antes de formar el mensaje.
Por eso el reporte mostraba paréntesis vacíos aunque `compileAndRun` sí registraba la
causa.

## Cambio

- Se conserva primero el resultado de O0 y después se forma su mensaje.
- Se aplica la misma secuencia a O2 para mantener el arnés coherente.
- No cambia la expectativa: ambos ejecutables todavía deben devolver 42.
- No cambia el fixture NativeSharp ni el compilador.

## Archivo

- `tests/unit/Item339ComVtableProductionTests.cpp`

## Validación dirigida

La primera repetición expuso la causa completa en ambas variantes: `NS3020` clasificaba
`table` como retenido globalmente al guardarlo en `instance->VTable`. Esa evidencia
permitió corregir el análisis de pointees en la tanda universal 13. Después de esa
corrección, `nsc.item339.com-vtable-production` pasó 2/2.

No se ejecutó baseline ni se usó la variante O2 para ampliar el alcance de implementación.
