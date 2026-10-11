# Tanda de validación 194 — TF-HIR-188

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el fixture de `TF-HIR-188`, perteneciente a
`CF-HIR-024`. No modifica el verificador HIR, el compilador, el plan ni el
manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El caso pretendía comprobar el rechazo de una relación inválida en una
declaración de función, pero representaba esa relación con `first == nullptr`.
El productor admite legítimamente ese estado como prototipo sin cuerpo, por lo
que el grafo era válido aunque el oráculo exigía rechazo.

## Corrección

El fixture conserva el escenario negativo y coloca un nodo `Constant` donde
la relación de cuerpo sólo admite un bloque. Así prueba una relación realmente
incompatible sin cambiar el contrato público. El plan y el manifiesto ya
describían esta intención y permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
