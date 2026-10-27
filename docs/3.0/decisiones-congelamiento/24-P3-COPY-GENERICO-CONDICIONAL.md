# Congelamiento 3.0 P3 — un `copy struct` genérico es Copy solo si sus argumentos lo permiten

Fecha: 2026-10-26. Revisión de partida: `af3a4bc99`.

## Problema

`KeyValuePair<TKey, TValue>` es un `copy struct`. Con una clave o un valor move-only (un struct con destructor),
la especialización fallaba al construirse: «copy type 'KeyValuePair__g2__Guard__int…' contains non-Copy field
'Key'». `SortedList`, `SortedDictionary` y `Dictionary` la nombran en sus firmas (`ToArray`, enumeradores), así
que no se podían instanciar con tipos move-only aunque no se usaran esas operaciones. Hallazgo de P2 (doc 18).

`typeIsImplicitlyCopyable` ya deriva la copiabilidad de los campos. Lo único que lo impedía era la validación de
la declaración `copy`, que se aplicaba igual a cada especialización.

## SCOPE LOCK

- **Cambia.** `BuildSupport.cpp`, `validateCopyType`: en una especialización genérica, un campo no `owned` que no
  es Copy ya no es error. El tipo cerrado queda move-only, y lo dicen las consultas que ya existían. Las
  especializaciones se reconocen por la marca `__g` del nombre, como en el resto del compilador.
- **Idéntico.**
  - Un `copy struct` no genérico con un campo no Copy sigue rechazado.
  - Un campo `owned` o un destructor en un `copy struct` siguen rechazados en toda especialización.
  - `KeyValuePair<int, int>` sigue siendo Copy.

## Validación

nsc Release recompilado. Sondas en `A:\temp\nsc-dec\d14`:

| Sonda | Resultado |
|---|---|
| K2CopyStays (`Pair<int,int>` copiado) | 35 |
| K3MoveOnlySpecialization (`Pair<Guard,int>` movido) | 7207 (una sola destrucción) |
| K4CopyRejected (`Pair<Guard,int> b = a;`) | «move-only value … cannot be copied into local 'b'» |
| K5NonGeneric (`copy struct Bad { Guard G; }`) | sigue rechazado |
| K1SortedListGuard (`SortedList<Guard,int>` con `InComparison`) | 123120413: orden 1-2-3, una destrucción al quitar y dos al final |

Casos `nsc_component_tests` de copia y move-only, en Debug y Release: TL-LANG-022, 176, 217, 252, 313, 354, 364,
366, 409, 410, 452, 453, 515, 519 y 521.

## Límite observado (no se corrige aquí)

`SortedList<TKey, …>` exige que `TKey` tenga `<` aunque se use un comparador, porque el respaldo `a < b` está en
el mismo cuerpo que la llamada al comparador. Eso aplica igual a claves copiables.
