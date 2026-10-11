# Tanda de validación 230 — cierre genérico seguido de asignación

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

El lexer entrega `>>=` como un solo token. `consumeGenericGreater` sólo sabía
descomponer `>>`, de modo que una lista anidada no podía consumir sus dos
cierres y conservar el `=` para el parser de expresiones. Además,
`TP-PAR-024` intentaba observar un token físico `Equal` que el lexer, por
contrato, nunca crea en esa entrada.

## Corrección

- El parser conserva los dos cierres genéricos mediante
  `pendingGenericGreater_` y publica después un token lógico `Equal` con el
  rango y lexema de la porción final del token compuesto.
- `current`, `peek` y `consume` respetan ese residuo lógico sin modificar el
  vector de tokens propiedad del lexer.
- El lookahead reconoce `GreaterGreaterEqual` como dos cierres.
- `TP-PAR-024` ejerce ahora `parseType` y comprueba tanto la estructura
  genérica completa como el `Equal` residual observable por el parser.

## Alcance

La causa corrige el productor compartido por `TP-PAR-024` y el contrato ya
documentado de `TP-PAR-098`. No se modificó la tokenización de operadores.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
