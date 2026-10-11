# Tanda de validación 229 — lifetime en lookahead de tipos

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

`Parser::scanTypeAt` consumía el calificador `borrowed`, pero dejaba sin
consumir su anotación opcional `[lifetime]`. El parser productivo sí reconoce
esa anotación mediante `parseOptionalLifetimeAnnotation`, por lo que el
lookahead terminaba en una frontera distinta de la que posteriormente usaba
`parseType`.

## Corrección

El lookahead conserva la identidad del calificador y, únicamente para
`borrowed`, valida y consume el triplete delimitado `[nombre]`. Los nombres
contextuales `this` y `static` siguen el mismo contrato aceptado por el parser
productivo. Una anotación incompleta no se acepta como tipo completo.

## Alcance

La causa corresponde a `TP-PAR-020`. No se modificaron el lexer, el AST ni los
consumidores semánticos.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
