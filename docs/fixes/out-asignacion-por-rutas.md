# Asignación de `out` en todas las rutas (NSC-NCSS-006) y limpieza léxica de GoBoard (NSC-NCSS-008)

## NSC-NCSS-006

La reproducción original de AlphaBeta (ejemplo 03) no se conserva. Se construyó la búsqueda que
describe la ficha: alfa-beta recursiva que devuelve la puntuación por valor y el movimiento por
`out`, más la versión por valor (`SearchResult`). Compila y da el resultado esperado en O0 y O2
(77). La variante con la rama base sin asignar el `out` también compilaba y devolvía 3: el
compilador solo comprobaba que el parámetro se escribiera en algún punto del cuerpo, como dice el
comentario C4 de `verifyOutParameterAssignments`, que lo dejaba como tarea descendiente. Con `int`
pasaba lo mismo.

El cambio añade la parte por rutas sobre el flujo que ya existe:

- `activeOutParameterAssignments_` guarda los `out` asignados en la ruta actual. Forma parte de
  `OwnershipFlowSnapshot` y se une por intersección, igual que los campos `required` de un
  constructor.
- Se marca en los mismos dos sitios que registran la escritura: asignación con raíz en el
  parámetro y paso como argumento `out`.
- `checkOutParametersAtExit` se ejecuta en cada `return` alcanzable y en el final alcanzable del
  cuerpo. Una ruta que termina en `throw` o `panic` no es una salida normal. Las lambdas no cuentan
  como salida del método. Mensaje: "out parameter 'x' is not assigned on every path that leaves
  'F'". La comprobación de cuerpo completo sigue para el parámetro que nunca se escribe y omite lo
  ya informado.
- Como antes, escribir un campo del `out` cuenta como escribirlo; la asignación campo por campo de
  un struct no se exige completa.

`tests/integration/out_parameter_paths.py` (`nsc.compiler.out-parameter-paths`): los programas
válidos (ambas ramas, retorno temprano que asigna, bucle tras asignar, `while (true)` con `break`,
`throw`, `panic`, try/catch, reenvío por `out`, lambda, campos de struct y la búsqueda alfa-beta)
dan 35 y 77 en O0 y O2. Seis inválidos, cada uno con una ruta sin asignar (retorno temprano, `if` sin
`else`, solo dentro de un bucle, `catch` sin asignar, campos en una sola rama y la búsqueda con caso
base vacío), se rechazan con el mensaje exacto.

## NSC-NCSS-008

La hipótesis era una fuga de los tableros clonados en `TacticalSearch.ns` y `Program.ns`, que se
declaran sin `raii` y se abandonan con `continue`. Desde el congelamiento 3.0 (elemento 141) un
local ligado directamente a `new` es su dueño y se libera al salir del ámbito.
`tests/integration/scope_cleanup_counters.py` (`nsc.compiler.scope-cleanup-counters`) construye
tableros en bucles que salen por `continue`, `break` y `return` temprano, con un nivel anidado, y
exige igualdad de construcciones y destrucciones tras cada llamada y los conteos exactos (5, 8,
7 y 14). Pasa en O0 y O2 (salida 42). No hay fuga ni doble liberación en esas formas.
