# Congelamiento 3.0 P2 (DEC-12) — delegados `in` y operaciones por intercambio

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `a3e8ad5fd`.

## Propósito y problema

`Comparison<T>`, `Predicate<T>` y `Action<T>` reciben sus operandos por valor. Llamarlos con un elemento de la
colección lo copia, y un `T` move-only (un struct con destructor o con campos `owned`) no lo permite. La sonda L1
(`list.Sort((Guard a, Guard b) => a.Id - b.Id)`) fallaba dentro de la biblioteca con un NS3020.

Además, `Algorithm.Sort`, `Reverse`, `RemoveIf` y `StableSort` movían elementos copiándolos
(`UncheckedAt`/`UncheckedSet`). Con un `T` con destructor, eso no compila.

Decisión de Alberto Huerta Luna (P2, 2026-10-26):

- Las APIs con delegados por valor exigen `T : copy`.
- Se agregan `InComparison<T>`, `InPredicate<T>` e `InAction<T>` y todos los overloads enumerados.
- `Sort`, `Reverse` y `RemoveIf` operan mediante move/swap.

## SCOPE LOCK

- **Objetivo.**
  - Toda API de la biblioteca que pasa un elemento a un delegado por valor exige `T : copy`.
  - Cada API enumerada tiene un overload `In*` que presta el elemento.
  - `Sort`, `Reverse`, `RemoveIf` y `StableSort` reubican elementos sin copiar ni destruir.
- **Naturaleza.** Cambio de biblioteca autorizado por P2. Usa `where` en miembros y constructores (P1).
- **Debe quedar idéntico.**
  - El resultado de toda operación con `T` copiable: orden, valores, conteos.
  - El orden relativo de los elementos que conserva `RemoveIf`.
  - Las APIs sin delegado.
- **Modificable.** `NativeSharp.Core.Collections.Generic`:
  - `Delegates.ns`, `Algorithm.ns`, `List.ns`;
  - `SortedList.ns`, `SortedSet.ns`, `SortedDictionary.ns`;
  - `PriorityQueue.ns`, `Dictionary.ns`, `HashSet.ns`, `Comparer.ns`.

  También `Freeze30ContractTests.cpp`.
- **Prohibido.**
  - Accesos por referencia (P3).
  - Mapa de vida en `Span` (P4).
  - Cambiar el compilador.
- **Terminación.** Sondas A1–A11 en `-O0` y `-O2`; c27 y c35 de cc1 en 42.

### Enmienda 2026-10-27 — plantilla pública de `Algorithm`

- **Entrada exacta:** un consumidor importa la plantilla pública de `Algorithm` desde
  `NativeSharp.Core.Collections.Generic`.
- **Defecto:** la plantilla conserva llamadas a `IntercambiarPalabras` e `IntercambiarMedias`, pero esos helpers
  privados no genéricos no forman parte de la plantilla publicada.
- **Cambio autorizado:** mantener el intercambio dentro del helper genérico publicado, usar únicamente
  identificadores de código en inglés y retirar los dos helpers ausentes.
- **Debe quedar idéntico:** API pública, ABI, selección por tamaño, orden de bytes, ownership y destrucciones.
- **Archivos modificables:** `Algorithm.ns` y este documento causal.
- **Protegido:** compilador, formato NSMETA, generador de snapshots, consumidores y pruebas existentes.
- **Validación:** inspección del diferencial y compilación externa de las bibliotecas y `SaruDX11`; no se ejecutan
  suites, baterías ni baselines en esta tanda.

## Contrato

1. **Delegados nuevos** (`Delegates.ns`):
   - `InComparison<T>(in T a, in T b)`;
   - `InPredicate<T>(in T value)`;
   - `InAction<T>(in T value)`.
2. **`T : copy` en toda API con delegado por valor.**
   - `Algorithm`: `FindIf`, `AnyOf`, `AllOf`, `RemoveIf`, `Sort(span, Comparison)` y `StableSort`.
   - `List`: `Sort(Comparison)`, `FindIndex`, `Find`, `FindLastIndex`, `FindLast`, `FindAll`, `Exists`,
     `TrueForAll`, `RemoveAll` y `ForEach`.
   - `HashSet` y `SortedSet`: `RemoveWhere` y `ForEach`.
   - Constructores con `Comparison`: `SortedList` (2), `SortedSet`, `SortedDictionary` y `PriorityQueue`.
   - Constructores con `HashFunc`: `Dictionary` (2), `HashSet` (2) y `EqualityComparer`.
   - Los dos constructores de `Comparer`.
   - `ForEach(KeyValueAction)` de `Dictionary`, `SortedList` y `SortedDictionary` exige `TKey : copy` y
     `TValue : copy`.
3. **Overloads `In*`.** Hay uno por cada API enumerada en la pregunta P2:
   - las diez de `List` (más `Sort`);
   - `RemoveWhere` y `ForEach` de los dos sets;
   - el constructor con `InComparison` de las cuatro colecciones ordenadas (dos en `SortedList`);
   - `FindIf`, `AnyOf`, `AllOf`, `RemoveIf`, `Sort` y `StableSort` de `Algorithm`.

   `Find`, `FindLast` y `FindAll` devuelven copias de elementos, así que sus overloads `In` siguen exigiendo
   `T : copy`. Los demás sirven para un `T` move-only.
4. **Comparadores guardados.** `SortedList` y `PriorityQueue` guardan siempre un `InComparison`.
   - El constructor con `Comparison` lo envuelve: `(in a, in b) => comparerFunc(a, b)`.
   - `KeyEquals` y `LowerBound` reciben sus claves `in`, así que comparar claves no copia ninguna.
5. **Intercambio.**
   - `Algorithm.Intercambiar` intercambia los bytes de dos elementos (de 8, 4 o 1 bytes según `sizeof(T)`).
     Es una reubicación: nada se copia ni se destruye.
   - No se puede escribir con `move`. Guardar por un `T*` destruye el valor sobrescrito, y el hueco que `move`
     acaba de vaciar sigue teniendo un `T` en cero. Medido: L5 daba 506, con dos destrucciones de más con
     `Id` 0, en vez de 306.
   - `Sort`, `Reverse`, `RemoveIf` y `StableSort` usan solo `Intercambiar`.
   - `StableSort` avanza el elemento por intercambios adyacentes: mismas comparaciones, mismo resultado.
   - `RemoveIf` deja los quitados detrás de la nueva longitud, intactos. Antes quedaban copias viejas.
6. **Sets.**
   - `SortedList` y `Dictionary` ganan `ForEachKey(InAction<TKey>)` y `RemoveKeysWhere(InPredicate<TKey>)`.
   - `Dictionary` gana el privado `RemoveEntryAt`.
   - Los overloads `In` de los sets los usan, porque `Keys()` copia.

## Validación

Sondas en `A:\temp\nsc-dec\d13`, en `-O0` y `-O2`:

| Sonda | Qué mide | Resultado |
|---|---|---|
| A1ListInSort | `List<Guard>`: `Sort(InComparison)`, `Sort()` con `operator <(in, in)`, `Reverse` | 321 (orden y 3 destrucciones) |
| A2ListInQuery | `FindIndex`, `FindLastIndex`, `Exists`, `TrueForAll`, `RemoveAll` con `InPredicate` | 131124813 |
| A3L1Rejected | L1: `Sort` con lambda por valor sobre `Guard` | «generic parameter 'T' of 'List.Sort' requires copyable storage…» |
| A4CopyableApis | APIs por valor con `int` y con una clase | 185212311 |
| A5Algorithm | `Algorithm` sobre `Span<Guard>` | 1114231 |
| A6SortedListIn | `SortedList<int,int>(InComparison)`, `ForEachKey`, `RemoveKeysWhere` | 321120000 |
| A7PriorityIn | `PriorityQueue<int,Guard>(InComparison)` | 1020123 |
| A8SetsIn | `HashSet`/`SortedSet`/`Dictionary` con overloads `In`; `SortedDictionary(Comparison)` | 2432117 |
| A9CtorRejected | `new PriorityQueue<int,Guard>(Comparison<Guard>)` | «'PriorityQueue constructor' requires copyable storage» |
| A10Strings | `StableSort`, `Sort(Comparison)` y `Reverse` con `string` | 1233211 |
| A11Count | destructor que cuenta llamadas: `List` y `Algorithm` sobre `Guard` | 11306024 (cero destrucciones de más) |

- cc1 `c27_algoritmos` y `c35_sobrecarga_generica`: 42 y 42.
- L5SpanReverseStruct: 306. Con la biblioteca de `HEAD` no compilaba (NS3020 en `Algorithm.ns:119`).
- L1SortStruct: rechazada con el diagnóstico de arriba.
- L3 y L4 (`CopyTo`, `ToArray`) siguen fallando dentro de la biblioteca. Son de P3.
- Sondas de DEC-11 con `List`: sin cambios.
- `examples/project_stage52` termina con 0xC0000374 (corrupción del heap) también con la biblioteca de `HEAD`
  extraída con `git archive`. Es anterior a este cambio.

## Hallazgos (no corregidos aquí)

- **Indexador de `List` (para P3).** `list[0].Id` sobre `List<Guard>` destruye una copia temporal: A1b da 70777,
  con dos destrucciones del mismo valor. El getter `owned T` devuelve el elemento por valor.
- **`KeyValuePair` (para P3 y los elementos 91–96).** `KeyValuePair<TKey,TValue>` es un `copy struct`. Con una
  clave o un valor move-only, `Dictionary`, `SortedList` y `SortedDictionary` no se pueden instanciar: «copy type
  … contains non-Copy field». Por eso A6 y A8 usan `int`.
- **Escritura por `T*` (para P4).** Guardar por un `T*` destruye el valor anterior aunque sea un hueco vacío.
- **Lambdas `in` en un `new`.** Una lambda `in` como argumento directo de un `new` necesita una local (ver doc
  17).
- **`raii string[]` de literales.** Una sonda con `raii string[] s = new string[] { "x", "y" }` terminó con
  0xC0000374. No se investigó: no toca P2.

## No verificado

- `Freeze30ContractTests` no está configurado en los árboles locales; `testInDelegateOverloads` repite A11, A2,
  A7, A8, A3 y A9.
- `selfhost` (usa `StableSort<string>` con `Comparison`): cubierto por A10, sin ejecutar el host.
- Batería completa: queda para el cierre final.
