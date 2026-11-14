# Ciclos en la regla conservadora de vida de arreglos (M2-05)

Rama `development-main-fixes`. Continúa el contrato de `04-regla-conservadora-de-vida-en-arreglos.md`.
Evidencia: `A:\nsc-main-fixes-evidencia\claude\logs\m2-05-*` y
`A:\nsc-main-fixes-evidencia\claude\probes\RESUMEN-m2-05.txt` (sondas P23 y P24).

## SCOPE LOCK

- Objetivo: cerrar el límite «los ciclos se analizan en una pasada» del documento 04. Un evento de vida en
  una iteración debe alcanzar los usos al inicio de la siguiente.
- Naturaleza: funcional (amplía los rechazos E2 de la regla conservadora autorizada por el usuario).
- Debe permanecer idéntico:
  - todo callable que el replay no reproduce;
  - todo veredicto distinto de «sin evento de vida» (la pasada solo agrega E2);
  - los diagnósticos precisos existentes (E1, ciclo, campo, copia, prueba tardía).
- Archivos modificables:
  - `ArrayReferenceFlowState.h`, `ProjectMethodBodyModel.h`;
  - `StatementAnalyzer.cpp`, `IndexAndRangeExpressions.cpp`, `MethodBodyCallableLifecycle.cpp`;
  - `NativeSharp3T06EscapeSummaryTests.cpp`;
  - este documento.
- Prohibido: merge, rebase, push, despliegue, batería completa, reconstrucción completa repetida.

## Causa

El replay analiza el cuerpo de un ciclo una sola vez. En este programa:

```
for (...) {
    if (i > 0) { total += refs[0].Get(); }
    MyClass item = new MyClass();
    Store(item, refs);
}
```

La lectura precede, en el cuerpo, a la limpieza de `item` al final del bloque. Por eso la primera pasada
no ve ningún evento de vida. Sin embargo, desde la segunda iteración la lectura usa el objeto de la
iteración anterior, que ya fue destruido. El compilador anterior aceptaba la sonda P23
(`nsc-conservadora.exe`: «ok»).

## Contrato

1. **Registro.** Durante el replay, cada evento de vida queda registrado con la sentencia más interna que
   lo produjo:
   - limpieza de un dueño (efecto `Destroyed` o fin de bloque; en el fin de bloque, el sitio es el bloque);
   - `move` sin resultado demostrado;
   - liberación de lo adoptado.

   Cada guardado en un arreglo rastreado (escritura cierta o posible) queda registrado con su sentencia,
   su ranura y su objeto. No se registra nada en un camino detenido.
2. **Eventos que llegan al regreso del ciclo.** Para cada ciclo (`for`, `while`, `do`, `foreach`) que se
   puede entrar, se toman los eventos cuyo sitio está dentro del ciclo. Se excluyen los que, en su
   secuencia (bloque o sección de `switch`), van seguidos de `return`, `throw` o un `break` de ese mismo
   ciclo; un `break` de un ciclo o `switch` interior no excluye.
3. **Usos afectados.** Un uso dentro del ciclo que la primera pasada dejó sin evento de vida pasa a
   «vida no demostrada» (E2) si se cumple alguna de estas condiciones:
   - algún objeto que puede nombrar tiene un evento del punto 2;
   - la ranura no estaba escrita en todo camino antes del uso, y dentro del ciclo se guardó en el mismo
     arreglo (misma ranura, o índice desconocido en alguno de los dos lados) un objeto con un evento del
     punto 2.
4. **Escritura en la misma iteración.** Si la ranura se escribió con índice literal en todo camino antes del
   uso, el uso nombra el valor de esa iteración. Los objetos creados dentro del ciclo se excluyen, porque
   cada iteración crea uno nuevo. Los objetos creados fuera del ciclo con evento dentro siguen afectando.
5. La pasada nunca limpia un veredicto ni produce E1: una destrucción vista solo a través del regreso del
   ciclo no se considera demostrada.

## Pruebas (`nsc.ns3.t06.escape-summaries`, 431 comprobaciones, pasa)

- Se rechazan con E2:
  - `loop_previous_iteration_read` (`for`);
  - `loop_previous_iteration_while` (copia en `while`);
  - `loop_outer_owner_dropped` (un dueño externo se suelta en una iteración y se reinicializa).
- Se aceptan:
  - `loop_same_iteration` (crear, guardar y leer en la misma iteración);
  - `loop_drop_then_break` (soltar seguido de `break`);
  - `loop_unrelated_array` (el evento afecta a otro arreglo).
- Sondas:
  - P23 se rechaza con E2 en `Main.ns:21:42`;
  - P24 sigue siendo válida;
  - P01–P22 no cambian respecto de `RESUMEN-m2-02.txt`.
- Suites dirigidas también aprobadas: `nsc.stage50_7.borrow-escape-analysis`, `nsc.class-value-origins`,
  `nsc.stage56.c7-definite-assignment`.

## Límites

- La pasada es conservadora a nivel de ciclo. No distingue la primera iteración ni ciclos que solo pueden
  ejecutar una vez (salvo los nunca entrados del documento 04). Un programa así recibe E2 aunque sea
  seguro.
- `continue` no excluye eventos, porque sí llega al regreso del ciclo.
- Los recorridos `foreach` sobre el arreglo no son usos indexados. Quedan para la auditoría de rutas
  (M2-07).
