# Tanda de validación 231 — nombre promovido duplicado

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

El parser ya detectaba mediante su conjunto de nombres que dos parámetros
primarios promovían el mismo campo, pero publicaba el diagnóstico genérico
`ExpectedDeclaration`. La entrada sí es una declaración válida; el defecto es
la colisión de un símbolo ya declarado.

## Corrección

La frontera propietaria conserva la detección y recuperación existentes, pero
clasifica la colisión como `DuplicateSymbol`. El texto y el rango continúan
señalando el segundo parámetro promovido.

## Alcance

La causa corresponde a `TP-PAR-160`. No se modificaron la síntesis del campo,
el constructor promovido ni la resolución semántica posterior.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
