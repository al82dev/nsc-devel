# Tanda de validación 201 — TF-MIR-186 y TF-MIR-187

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente las fuentes de `TF-MIR-186` y
`TF-MIR-187`, pertenecientes a `CF-MIR-024`. No modifica el parser, el builder
MIR, el compilador, el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

Los escenarios de topología usaban cuerpos simples para `while` y `for`. La
gramática vigente exige bloques en ambos ciclos, de modo que las fuentes eran
rechazadas antes de generar los bloques MIR ordenados y terminados que el
oráculo pretendía recorrer.

## Corrección

Los cuerpos de ambos ciclos se expresan como bloques explícitos, conservando
el incremento, el `break`, el flujo y los resultados originales. El contrato,
el plan, el inventario y el manifiesto permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
