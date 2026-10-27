# Congelamiento 3.0 P3 — devolver por valor un lugar move-only es una copia

Fecha: 2026-10-26. Revisión de partida: `ffee8c564`.

## Problema

Hallazgo de Alberto sobre P3: `list[i].Id` sobre `List<Guard>` destruye el elemento dos veces. El getter de
`List<T>` hace `return items[index]`. Con `T` move-only (un struct con destructor), eso copia el elemento: el
llamador destruye la copia temporal, y la lista destruye el original al final.

La causa está en el compilador, no en `List`. NS3020 rechaza copiar un valor move-only a un local
(`Guard b = items[0]`) o a un parámetro por valor. `return` solo lo rechazaba cuando la expresión era un local o
un parámetro (`reportDirectOwnerReturnWithoutMove`). Un campo, un elemento de arreglo o una desreferencia pasaban
sin diagnóstico:

| Sonda (`A:\temp\nsc-dec\d15`) | Forma | Antes |
|---|---|---|
| C1ReturnCopy | `public Guard Get(int i) { return items[i]; }` | 7777 (dos destrucciones) |
| C2OwnedIndexer | indexador `owned Guard` con `get { return items[i]; }` | 7777 |
| C3GenericReturnCopy | `Holder<Guard>.Get` con `return items[i]` | 7777 |
| C7FieldReturn | `public Guard FromField() { return field; }` | compilaba |
| A1bIndexer (`d13`) | `list[0].Id` sobre `List<Guard>` | 70777 |

## SCOPE LOCK

- **Cambia.** `StatementAnalyzer.cpp`: `rejectMoveOnlyPlaceReturn`, llamada en `analyzeReturn` después de analizar
  el valor. Un `return` por valor de una expresión direccionable de tipo move-only, que no es `move(...)`, da
  NS3020: «move-only value of type 'T' cannot be copied into the return value; use return move(source) or a ref
  return».
- **Idéntico.**
  - El retorno por referencia (`return ref …`) se analiza antes y no pasa por aquí.
  - Un local o un parámetro conserva su diagnóstico de siempre.
  - Un tipo copiable (`copy struct`, primitivos, referencias) no cambia.
  - Una plantilla solo se analiza para los miembros que se usan, así que `List<Guard>` sigue sirviendo para
    `Add`, `RemoveAt` y lo demás. Solo `list[i]` falla, y lo resuelve el cambio de biblioteca de P3.

## Validación

nsc Release recompilado. Sondas:

| Sonda | Ahora |
|---|---|
| C1, C2, C3, C7 | NS3020 en el `return` |
| A1bIndexer | NS3020 en `List.ns:74` (pendiente del cambio de biblioteca) |
| D1RefReturn (`ref Guard Get`) | 77: una sola destrucción |
| D2CopyReturn (`copy struct`) | 4 |
| L2ReverseStruct, P2ListAdd, C12ListGrow, C12ListRemove, C12LinkedListT (`d11`) | 306, 42, 12345, 2213, 1123: sin cambio |
| A2ListInQuery, A4CopyableApis (`d13`) | 131124813 y 185212311: sin cambio |

Casos `nsc_component_tests` en Debug y Release: los 48 de referencias y préstamos (96 PASS) y los 15 de copia y
move-only (30 PASS). Corpus de `await` (51 programas): mismos resultados que `d04`.

Prueba nueva: `Freeze30ContractTests::testMoveOnlyPlaceReturn` (campo rechazado, elemento genérico rechazado y
retorno `ref` aceptado, 77).
