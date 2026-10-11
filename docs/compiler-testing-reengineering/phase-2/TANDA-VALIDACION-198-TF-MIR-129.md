# Tanda de validación 198 — TF-MIR-129

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente la fuente del fixture `TF-MIR-129`,
perteneciente a `CF-MIR-017`. No modifica el parser, el builder MIR, el
compilador, el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El caso pretendía comprobar los bloques y saltos explícitos de un `while`,
pero usaba una sentencia simple como cuerpo. La gramática vigente exige un
bloque para el cuerpo del ciclo, de modo que la fuente era rechazada antes de
alcanzar el lowering MIR.

## Corrección

El incremento queda dentro de un bloque explícito. La condición, el efecto y
el resultado del programa se conservan; sólo se elimina el rechazo sintáctico
ajeno al contrato MIR. Plan, inventario y manifiesto permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
