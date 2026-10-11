# Tanda de validación 203 — TF-MIR-267 y TF-MIR-271

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente los fixtures `TF-MIR-267` y `TF-MIR-271`,
pertenecientes a `CF-MIR-034`. No modifica el verificador MIR, el compilador,
el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

`TF-MIR-267` agregaba a la operación un valor `float` sin instrucción
definidora. El verificador rechazaba correctamente ese operando desconocido
antes de alcanzar el contrato aritmético que el oráculo quería observar.
`TF-MIR-271` declaraba `long Main()`, por lo que el frontend rechazaba la firma
del punto de entrada antes de producir MIR verificable.

## Corrección

El operando `float` se define ahora mediante una constante MIR con payload
coherente, dejando como primer defecto causal la incompatibilidad aritmética.
La conversión `long` se conserva en una función auxiliar y `Main` adopta la
firma válida. Plan, inventario y manifiesto permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
