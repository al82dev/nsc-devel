# Tanda de validación 185 — TP-PAR-200

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el oráculo de `TP-PAR-200` y sincroniza su
plan, inventario y manifiesto. No modifica el parser ni otras pruebas.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

`new class Nested` es una declaración anidada válida y el parser conserva
`KwNew`. En cambio, `static struct`, `readonly interface` y `const enum` son
combinaciones inválidas que deben diagnosticar. El oráculo anterior rechazaba
también la forma válida con `new`.

## Corrección

La prueba inspecciona el `ClassDeclaration` anidado, su modificador `KwNew` y
la ausencia de diagnóstico, y conserva los tres rechazos por kind. Se
sincronizaron plan, inventario y manifiesto.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
