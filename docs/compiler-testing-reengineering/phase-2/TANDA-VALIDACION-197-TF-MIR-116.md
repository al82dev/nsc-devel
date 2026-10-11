# Tanda de validación 197 — TF-MIR-116

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente la fuente y selección de función del fixture
`TF-MIR-116`, perteneciente a `CF-MIR-015`. No modifica el builder MIR, el
compilador, el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El caso de código posterior a terminador declaraba `void Main()`, firma que el
frontend rechaza antes de producir MIR. Además, el oráculo inspeccionaba
siempre `Main`, aunque el comportamiento que debía observar era una función
`void` con un `return` anticipado.

## Corrección

El cuerpo `void` se conserva en `F`, se agrega un `Main` válido que lo invoca y
el caso selecciona `F` para comprobar el bloque separado y terminado. Los
demás casos continúan inspeccionando `Main`. Plan, inventario y manifiesto ya
describían el contrato correcto y permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
