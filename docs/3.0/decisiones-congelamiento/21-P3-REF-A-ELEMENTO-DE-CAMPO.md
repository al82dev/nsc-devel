# Congelamiento 3.0 P3 — `return ref campo[i]` con la vida del receptor

Fecha: 2026-10-26. Revisión de partida: `b0a03bd98`.

## Problema

Un indexador `ref` de colección necesita `return ref items[index];`, donde `items` es un arreglo campo, o
`return ref start[index];` en un span con un puntero campo. El evaluador de escape informa un elemento de arreglo
o de puntero como almacenamiento externo de vida desconocida. La vida inferida de un retorno `ref` de instancia
es `'this'`, así que se rechazaba: «requires lifetime 'this' but the returned storage has lifetime 'unknown'»
(sondas R7Indexer y R13Foreach, también con `owned int[]`).

## SCOPE LOCK

- **Cambia.**
  - `ProjectMethodBodyModel::indexesReceiverFieldStorage` (declarada en `ProjectMethodBodyModel.h` y definida en
    `StatementAnalyzer.cpp`).
  - `analyzeReferenceReturn`: con vida `'this'`, un `campo[i]` (implícito o `this.campo`) cuyo campo es un arreglo
    nativo o un puntero del receptor se acepta.
- **Idéntico.**
  - El evaluador de escape.
  - Los locales `ref`.
  - Las reglas de marco y de parámetro.
  - Un elemento de un arreglo local o de un parámetro sigue rechazado.
- **Límite que se documenta.** La referencia vale mientras el almacenamiento del receptor no se reemplace. Una
  operación que reubica el arreglo (`List.Add` que crece) invalida las referencias previas, como en C++. El
  préstamo léxico de `foreach (ref …)` impide llamadas mutantes durante el recorrido (NS3-T09-F04). Un local
  `ref` fuera de un `foreach` no tiene esa protección.

## Validación

| Sonda | Resultado |
|---|---|
| R7Indexer (indexador y propiedad `ref readonly` sobre `owned int[]`) | 654 |
| R13Foreach (`Current` `ref readonly` sobre `owned int[]`) | 123 |
| R8IndexerWrite, R14ForeachMut | siguen rechazadas por ser readonly, ya no por la vida |
