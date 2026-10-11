# Tanda de validación 200 — TF-MIR-178

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente la fuente del fixture `TF-MIR-178`,
perteneciente a `CF-MIR-023`. No modifica el builder MIR, el compilador, el
plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

La prueba de lowering de primitivas declaraba `long Main()`. La regla del punto
de entrada exige retorno `int`, así que el frontend rechazaba el programa antes
de crear la función y el terminador MIR que el oráculo debía comprobar.

## Corrección

La conversión a `long` se conserva en una función auxiliar y `Main` usa una
firma válida, convirtiendo su resultado a `int`. El escenario sigue
ejercitando conversiones primitivas y retorno explícito. Plan, inventario y
manifiesto permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
