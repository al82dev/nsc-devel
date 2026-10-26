# Congelamiento 3.0 P3 — biblioteca: acceso por referencia y copias explícitas

Fecha: 2026-10-26. Revisión de partida: `b50a07746`.

## Decisión aplicada

P3, opción 1, en palabras de Alberto: accesos por referencia y enumeración por referencia; la referencia es de solo
lectura donde mutarla rompería invariantes (claves, elementos de conjuntos); las operaciones que necesariamente
devuelven una copia exigen `T : copy`.

Hallazgos que lo motivan: `list[i].Id` sobre `List<Guard>` destruía el elemento dos veces (corregido en el
compilador en el doc 25), y las colecciones solo servían para meter y sacar un `T` move-only.

## Diseño

### El indexador de `List<T>` sigue siendo `owned T` con get y set

La opción 1 decía «un indexador `ref` en `List`». Lo medí y no se puede sin romper contratos vigentes
(sondas en `A:\temp\nsc-dec\d15`):

- Un indexador `ref` no puede tener setter (regla de Item314, con prueba histórica que lo exige).
- `list[i] = move(obj)` con `T` clase es el contrato del item 9 (`Stage50_11`, comentario de `List.ns`). Por un
  `ref` el `move` se rechaza: el store por referencia no es contexto que consume (B1).
- `list[i] = new Box(…)` por un `ref` se acepta pero no destruye el objeto anterior (B2: 320032, el `Box(1)` se
  pierde). El setter `owned` sí lo destruye.

Por eso el indexador conserva get y set. El getter copia y lleva `get where T : copy` (doc 26). El setter no copia
y sirve para todo `T`. El acceso sin copia es el método nuevo `RefAt(int)`.

### Acceso por referencia (todo `T`)

| Tipo | Miembro |
|---|---|
| `Span<T>` | `ref T RefAt(int)` |
| `ReadOnlySpan<T>` | `ref readonly T RefAt(int)` |
| `List<T>`, `Stack<T>`, `Queue<T>` | `ref T RefAt(int)` (orden lógico, igual que el indexador) |
| `PriorityQueue<TElement, TPriority>` | `ref TElement RefAt(int)` |
| `Stack`, `Queue`, `PriorityQueue` | `Peek()` devuelve `ref` |
| `SortedSet<T>` | `Min()`, `Max()` y `RefAt(int)` devuelven `ref readonly` |
| `SortedList<TKey, TValue>` | `KeyAt` devuelve `ref readonly TKey`; `ValueAt`, `ref TValue` |
| `LinkedListNode<T>` | `Value()` devuelve `ref T` |

`RefAt` fuera de rango hace `panic` (exit 142): no hay elemento al cual referir.

### Enumeración por referencia

Los enumeradores vivos (`List`, `Stack`, `Queue`, `PriorityQueue`, `LinkedList`) exponen `ref T Current`.
`foreach (ref T x in c)` llega al elemento sin copiar y `foreach (ref mut T x in c)` lo modifica. Un `foreach`
por valor copia, así que con un `T` move-only da NS3020 en la línea del usuario.

Los enumeradores por instantánea (`SortedSet`, `HashSet`, `Dictionary`) copian todos los elementos al empezar. No
los convertí en vivos porque eso cambiaría lo que ve un ciclo que modifica la colección mientras la recorre. Su
`GetEnumerator` y su constructor exigen `copy`. Para un `T` move-only quedan `RefAt`, `Min`, `Max`, `KeyAt`,
`ValueAt`, `ForEachKey` y los delegados `In*` de P2.

### Lo que copia exige `copy`

- `List`: getter, `GetRange`, `ToArray`, `CopyTo`.
- `Stack`, `Queue`: getter, `TryPeek`, `ToArray`, `CopyTo`.
- `PriorityQueue`: getter, `TryPeek` (`TElement` y `TPriority`), `TryDequeue` (`TPriority`).
- `Span`: getter, setter, `UncheckedAt`, `UncheckedSet`, `Fill`, `CopyTo`, `CopyToOverlapping`.
  `ReadOnlySpan`: getter, `UncheckedAt`, `CopyTo`.
- `SortedSet`: `ToArray`, `Reverse`, `GetViewBetween`, las diez operaciones de conjuntos y `GetEnumerator`. Su
  implementación trabaja sobre instantáneas; reescribirlas sin copias no es parte de P3.
- `SortedList`, `SortedDictionary`, `Dictionary`: getter y `TryGetValue` (`TValue`), `Keys`, `Values`,
  `ToArray`; `Dictionary.GetEnumerator`.
- `LinkedList.ToArray`.

### Búsquedas sin copia

Una búsqueda compara elementos en su lugar con el `==`/`<` del tipo (con operandos `in`) y no devuelve una copia,
así que sirve para todo `T`:

- `List.IndexOf`, `LastIndexOf`, `Contains`, `Remove(T)`, `BinarySearch`; `Contains` y `Remove` prestan su
  parámetro a `IndexOfIn(in T)` en lugar de copiarlo.
- `SortedList.FindIndex` recibe la clave `in`; `IndexOfKey`, `ContainsKey`, `SetValue` y `Remove` ya no copian.
- `SortedSet.Add`, `Contains`, `Remove` y los reenvíos de `SortedDictionary` entregan su parámetro con
  `move(...)`, que vale para todo `T` (sonda G2: 1103). `SortedSet.Add` usa `SortedList.TryAdd`, nuevo, igual
  que `Dictionary.TryAdd`.
- `Algorithm.Find` compara por el puntero del span en vez del getter que copia.
- `LinkedList.Find`, `FindLast`, `Remove(T)` y `Contains` ya no copian porque `Value()` devuelve referencia.
  `Stack.Contains` y `Queue.Contains` nunca copiaban.

## Validación

nsc Release con los docs 25 a 27. Sondas en `A:\temp\nsc-dec\d15`, `-O0` y `-O2`; el destructor de `Guard` cuenta
destrucciones:

| Sonda | Resultado | Lectura |
|---|---|---|
| H1ListRefAt (`RefAt`, escritura por `RefAt`, `foreach (ref …)`) | 779002 | 7, 9 en su lugar; 2 destrucciones |
| H2ListCopyable (`List<int>` get/set/`foreach`/`ref mut`, `List<Box>`) | 151584034 | 15, 151, 1515, `ref mut` a 2 y 6, `Box` 3 destruido al reemplazar |
| H3PeekRef (`Peek` por `ref` en las tres colas, escribir por `Peek`, `foreach (ref …)`) | 4513404 | 4 destrucciones |
| H4SortedSetMoveOnly (`Add` repetido, `Min`, `Max`, `Contains`, `Remove`) | 1311105 | 5 destrucciones, una por objeto |
| H5SortedListMoveOnly (`KeyAt`, `ValueAt` escrito, `ContainsKey`, `Remove`, `SortedDictionary`) | 6711108 | 8 destrucciones, una por objeto |
| H6LinkedListMoveOnly (`Value()` escrito, `Contains`, `Find`, `foreach (ref …)`) | 3113204 | 4 destrucciones |
| H7SpanRefAt (`RefAt` de `Span` y `ReadOnlySpan`; `Span<int>` get/set/`Fill`/fuera de rango) | 5267602 | 52; `Span<int>` 676 con `t[5]` = 0 (indexador total) |
| H8RefAtPanic (`RefAt(1)` con un elemento) | 142 | panic |

Sondas F1–F10 (cada miembro de cada colección con `Guard`): todo lo que copia se rechaza en la línea del usuario
con «requires copyable storage ('where T : copy')», y un `foreach` por valor con NS3020. Ninguna queda dentro de
la biblioteca.

Regresión con la biblioteca nueva (mismos valores que en los docs 15, 18 y 24):

- `d13`: A1 321, A2 131124813, A4 185212311, A5 1114231, A6 321120000, A7 1020123, A8 2432117, A10 1233211,
  A11 11306024; A3 y A9 rechazadas con el mismo mensaje; cc1 `c27_algoritmos` y `c35_sobrecarga_generica` 42.
- `d12`: L5SpanReverseStruct 306; L1 rechazada igual; L3 y L4 (`CopyTo`, `ToArray` con `Guard`) ahora se
  rechazan en la línea del usuario con el constraint, antes fallaban dentro de la biblioteca.
- `d11`: L2 306, P2 42, C12ListGrow 12345, C12ListRemove 2213, C12LinkedListT 1123. `d14`: K1 123120413.
- Casos `nsc_component_tests` Debug y Release: 96 PASS de referencias, 30 PASS de copia. Corpus de `await`: igual
  que `d04`.

Pruebas nuevas: `Freeze30ContractTests::testCollectionReferences` y `testCollectionReferencesSorted` (H1–H8 y el
getter de `List<Guard>` rechazado).

## Límites

- Una escritura por `RefAt`, `Peek`, `ValueAt` o `Value()` sobre un `T` clase reemplaza la referencia sin
  destruir la anterior, como cualquier `ref` a un slot `owned` (B2). Para reemplazar se usa el setter, `SetAt`,
  `AddOrUpdate` o `SetValue`.
- Un `Span` sobre un arreglo con mapa de vida (DEC-11) todavía no lo consulta: es P4.
