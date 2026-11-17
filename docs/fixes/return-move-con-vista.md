# `return move(dueño)` mientras vive un préstamo del dueño

## Problema

`validateLocalMoveState` rechazaba `return move(owner);` cuando seguía vivo un
préstamo (alias `borrowed` o vista) del local dueño. Sin embargo, ese préstamo no
puede usarse después del `move`: el `return` termina todos los locales del callable,
vistas y locales prestados incluidos. El patrón aparece en las bibliotecas (fábricas
que validan el objeto a través de un alias y luego lo devuelven).

## Causa

`hasActiveBorrowOfOwnerLocal` mira el ámbito y no distingue un `move` que es todo el
valor del `return`, tras el cual ningún código del callable vuelve a ejecutarse.

## Cambio

- `StatementAnalyzer.cpp` (`analyzeReturnStatement`) registra en
  `directReturnMoveOperand_` el `MoveExpression` que es todo el operando del
  `return`, fuera de lambdas, y restaura el valor anterior al terminar.
- `MoveExpressions.cpp` (`validateLocalMoveState`) acepta ese operando antes de
  consultar los préstamos activos.
- `ProjectMethodBodyModel.h` declara el miembro.

## Comportamiento conservado

Un `move` que no es todo el valor de un `return` (por ejemplo
`raii Holder other = move(owner);` seguido de un uso del alias) se sigue rechazando.
Dentro de una lambda no se aplica: el `return` de la lambda no termina los locales
del callable que la contiene. El resto de las comprobaciones de
`validateLocalMoveState` (unión de flujo, movimientos previos) se evalúa antes y no
cambia.

## Pruebas

Nueva: `tests/integration/return_move_with_view.py`. Acepta y ejecuta (salida 42 en
O0, O1, O2 y O3) el retorno con alias vivo y con vista viva. Rechaza el `move` fuera
del `return` seguido del uso del alias.

## Evidencia

Pendiente de completar con los resultados finales.
