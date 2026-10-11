# Tanda de validación 199 — TF-MIR-143

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente la fuente del fixture `TF-MIR-143`,
perteneciente a `CF-MIR-018`. No modifica el parser, el builder MIR, el
compilador, el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El escenario de terminación de statements usaba un cuerpo simple en `while`.
La gramática vigente requiere un bloque, por lo que el frontend rechazaba la
fuente y nunca producía la función MIR cuyo bloque final debía inspeccionarse.

## Corrección

El cuerpo del ciclo se expresa como bloque explícito sin alterar la condición,
el incremento ni el valor retornado. El contrato MIR, el plan, el inventario y
el manifiesto permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
