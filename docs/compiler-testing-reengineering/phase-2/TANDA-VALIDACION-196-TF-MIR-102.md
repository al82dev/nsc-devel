# Tanda de validación 196 — TF-MIR-102

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente la fuente del fixture `TF-MIR-102`,
perteneciente a `CF-MIR-013`. No modifica el builder MIR, el compilador, el
plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El escenario nominal declaraba `long Main()`. El frontend exige que el punto
de entrada `Main` retorne `int`, por lo que la fuente era rechazada antes de
producir HIR y MIR. La falla observada no ejercitaba el contrato del builder.

## Corrección

La operación `long` se conserva en una función auxiliar y `Main` adopta la
firma válida, convirtiendo el resultado a `int`. El objetivo de producir una
unidad HIR y un módulo MIR completos no cambia; plan, inventario y manifiesto
permanecen vigentes.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
