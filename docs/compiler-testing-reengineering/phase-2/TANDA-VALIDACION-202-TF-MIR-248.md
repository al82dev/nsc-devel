# Tanda de validación 202 — TF-MIR-248

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente la fuente del fixture `TF-MIR-248`,
perteneciente a `CF-MIR-031`. No modifica el parser, el verificador MIR, el
compilador, el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El caso válido de escala del verificador de bloques usaba una sentencia simple
como cuerpo de `while`. La gramática vigente requiere un bloque, por lo que el
frontend rechazaba el programa antes de generar MIR verificable.

## Corrección

El incremento se encierra en un bloque explícito. El ciclo, su volumen y el
resultado permanecen iguales, mientras el escenario vuelve a alcanzar el
verificador MIR. Plan, inventario y manifiesto permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
