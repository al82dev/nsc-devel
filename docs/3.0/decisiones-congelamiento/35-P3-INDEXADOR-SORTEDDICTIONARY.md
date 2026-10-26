# Congelamiento 3.0 P3 — regresión: el indexador de `SortedDictionary` no compilaba

Fecha: 2026-10-26. Revisión de partida: `605114b3b`.

## Nota de trabajo

```text
HEAD: 605114b3b
Elemento: defecto reproducido durante P7 (elemento 111, compilación focal de un proyecto histórico)
Contrato vigente: DEC-12 / P3 (doc 28): acceso por referencia; una copia inevitable exige copy
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  lib/NativeSharp.Core.Collections.Generic/SortedDictionary.ns
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Al validar P7, `examples/tutorial_15_generic_collections_tour` ya no compilaba con el contenido actual:

```
SortedDictionary.ns:107:24: error: no viable Stage 38 overload of 'get_Item' accepts the operand/argument types
SortedDictionary.ns:107:30: error: move(...) is only valid for a direct RAII local or owned parameter, or a
move-only local/parameter, in a consuming context
```

La auditoría lo había medido compilando (`A:\temp\aud\check-head-new.txt`, `aa80795b4`: PASS). `e26ddb4bd` (P3)
cambió el getter de `items[key]` a `items[move(key)]` (`git log -S`). `move(...)` no es un argumento que consuma
en un acceso a elemento, así que el getter se rechaza con todo tipo de clave. Como los miembros genéricos se
analizan al usarse, las sondas de P3 no lo usaron y no lo vieron.

Sondas `A:\temp\nsc-dec\d22`:

| Sonda | Forma | Antes |
|---|---|---|
| I5SortedDictGet | `SortedDictionary<int, int>`, `d[1]` | rechazo de arriba |
| I6SortedDictTry | `TryGetValue(1, r)` (método, `move(key)` en una llamada) | 41 |
| I1IndexerMoveInt | `Inner(move(k))` dentro de un indexador de usuario | 41 |
| I7IndexerArgMove | `this[move(v)]` e `inner[move(k)]` | el mismo rechazo |

`move(...)` sí funciona como argumento de llamada; no como argumento de un acceso a elemento. Esa es una regla
del compilador que este commit no cambia.

## SCOPE LOCK

- **Cambia.** El getter del indexador de `SortedDictionary` vuelve a `return items[key];`, el texto de la base,
  con un comentario EN/ES.
- **Idéntico.** El resto de `SortedDictionary` y de la biblioteca; la regla de `move(...)` en el compilador.
- **Consecuencia aceptada.** Con un `TKey` move-only, el getter copia la clave y se rechaza con NS3020 al usarse.
  Es un rechazo explícito; `TryGetValue` sigue disponible.

## Validación

nsc sin recompilar (cambio sólo de biblioteca).

| Sonda | Ahora |
|---|---|
| I5SortedDictGet | 41 |
| I6SortedDictTry | 41 |
| `tutorial_15_generic_collections_tour` (`--check-project-oop-mir`) | `project OOP MIR check: ok (874 callables)` |
| d15 F6, F7, F8 (rechazos esperados con `Guard`) | los mismos diagnósticos `where … : copy` |

Regresión focal (`regress.cmd sdfix`): resultados de siempre.

Prueba nueva: `Freeze30ContractTests::testSortedDictionaryIndexerGet` (`d[1] + d[2] + d[3]` = 41; la clave 3 no
existe y devuelve `default(int)`). No se ejecutó; se ejecuta en el gate final.

## No verificado

- Batería completa: gate final.
