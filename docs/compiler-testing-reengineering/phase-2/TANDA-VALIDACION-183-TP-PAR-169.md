# Tanda de validación 183 — TP-PAR-169

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el oráculo de `TP-PAR-169` y sincroniza su
plan, inventario y manifiesto. No modifica el parser ni otras pruebas.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

La sintaxis de `variant` no admite discriminantes explícitos `Case = n`. El
parser diagnostica esa forma, recupera la declaración y sintetiza tags
ordinales monotónicos. El oráculo anterior trataba el discriminante como
válido y buscaba preservar `4`.

## Corrección

La prueba exige diagnóstico y verifica la recuperación exacta de `Zero` y
`One` con tags ordinales `0` y `1`. Se sincronizaron plan, inventario y
manifiesto.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
