# Tanda de validación 232 — alternativa variant duplicada

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

`parseVariantDeclaration` detectaba correctamente la segunda alternativa con
el mismo nombre y evitaba sintetizarla, pero publicaba
`ExpectedDeclaration`. La sintaxis de la alternativa es válida; el contrato
incumplido es la unicidad del símbolo dentro del `variant`.

## Corrección

La colisión se clasifica como `DuplicateSymbol`, conservando el rango del
segundo nombre y la recuperación que permite continuar con las alternativas
posteriores.

## Alcance

La causa corresponde a `TP-PAR-170`. No se modificaron los ordinales, el
payload ni los miembros sintetizados de alternativas únicas.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
