# Tanda de validación 233 — publicación de símbolos semánticos

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

`analyzeIdentifier` resolvía el símbolo y lo almacenaba en `ExpressionInfo`,
pero `SemanticAnalyzer::record` sólo publicaba la información de expresión.
Por ello `SemanticModel::symbol` no podía recuperar bindings válidos ya
analizados.

## Corrección

`record` publica también el símbolo cuando `ExpressionInfo` lo contiene. La
operación reutiliza `SemanticModel::bindSymbol`; los errores y expresiones sin
símbolo conservan una ausencia estable.

## Alcance

La causa corresponde a `TS-SEMA-100`. No se modificó lookup ni prioridad de
scopes.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
