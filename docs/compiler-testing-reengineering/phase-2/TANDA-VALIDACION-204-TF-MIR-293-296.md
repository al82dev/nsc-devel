# Tanda de validación 204 — TF-MIR-293 y TF-MIR-296

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente las fuentes de `TF-MIR-293` y
`TF-MIR-296`, pertenecientes a `CF-MIR-037`. No modifica el parser, el
verificador MIR, el compilador, el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

Los escenarios válidos de terminadores usaban cuerpos simples para `while`.
La gramática vigente exige bloques, por lo que ambas fuentes eran rechazadas
antes de construir los saltos y terminadores MIR que debían verificarse.

## Corrección

Los cuerpos con `break` e incremento se expresan como bloques explícitos. El
flujo y los resultados de ambos programas se conservan. El contrato, el plan,
el inventario y el manifiesto permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
