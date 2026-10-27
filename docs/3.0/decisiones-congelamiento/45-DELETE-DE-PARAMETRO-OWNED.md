# Congelamiento 3.0, elemento 121 — `delete` de un parámetro `owned` destruía dos veces

Fecha: 2026-10-26. Revisión de partida: `1a4cb8062`.

## Nota de trabajo

```text
HEAD: 1a4cb8062
Elemento: 121 del plan de 144 (Dictionary con valores destructibles), corregido según el elemento 130
Contrato vigente: C-01 (todo objeto tiene dueño hasta que se destruye una vez)
Documento histórico que podría contradecirlo: ninguno; DEC-06 (12-DEC06-DELETE-OWNED-ARRAY-LOCAL.md) ya fija
  que `delete[]` de un local `owned T[]` lo consume
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp
  src/Frontend/ProjectCallableResultMerger.cpp
  src/Frontend/HIR/ExpressionStatementMachine/HirInitializationValueFrames.cpp
  src/Frontend/HIR/HirExceptionAndCleanup.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Al verificar `Dictionary` con valores de clase con destructor (sondas `A:\temp\nsc-dec\d30`), `TryAdd` con una
clave repetida terminaba con `STATUS_HEAP_CORRUPTION` (-1073740940). `TryAdd` rechaza el par con
`delete key; delete value;`, que son parámetros `owned`. La forma mínima:

```text
static void Eat(owned Box b) { delete b; }
Eat(new Box(1));
```

`delete b` destruía el objeto y dejaba el puntero en el slot del parámetro. Al salir, la limpieza del parámetro
`owned` lo destruía otra vez. Con un struct no pasaba: `delete` de un valor struct no hace nada y el parámetro se
destruye una vez al salir (sonda Z7). Un local `raii`/`owned` de clase no llega aquí, porque su `delete` ya se
rechaza.

| Sonda | Forma | Correcto | Antes |
|---|---|---|---|
| Z1OwnedParamDelete | `delete b` de un parámetro `owned Box` | 1 destrucción | heap corrupto |
| Z2OwnedParamDeleteBranch | `delete b` en una sola rama | 1 | heap corrupto |
| BxTryAddFail | `Dictionary<int, Box>.TryAdd` con clave repetida | 5 de 5 | heap corrupto |
| SortedListBox | `SortedList<int, Box>.TryAdd` con clave repetida, entre otras | 9 de 9 | heap corrupto |
| Z7OwnedStructParamDelete | lo mismo con un struct | 1 | 1 |

Afecta a todo `TryAdd`, `AddOrUpdate` con clave de clase y `Add` de un conjunto que rechaza el elemento, con
elementos de clase.

## Contrato

`delete p` de un parámetro `owned` de clase o interfaz consume `p`, igual que `move(p)`:

1. Análisis. El parámetro queda movido. Leerlo o borrarlo otra vez da «use of moved-from parameter»; un
   `delete` en una sola rama lo deja «posiblemente movido» después de la unión. Un `delete` de un parámetro ya
   movido, o mientras un local prestado lo usa, se rechaza con el diagnóstico de `move`.
2. HIR. El `delete` direcciona el parámetro (`deleteClearsOwnedParameter`). LLVM carga el puntero, pone el slot en
   cero y destruye, como en el `delete items[i]` de un elemento referencia. La limpieza de salida del
   parámetro encuentra el slot vacío.

## SCOPE LOCK

- **Cambia.** Sólo `delete p` (escalar) cuando `p` es un parámetro `owned` de tipo referencia.
- **Idéntico.**
  - `delete` de un parámetro sin `owned`.
  - `delete` de un struct, que sigue sin efecto.
  - `delete[]`, los locales `raii`/`owned` (siguen rechazados), los campos y los elementos de arreglo.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas d30 en `-O0` y `-O2`: Z1 y Z2 dan una
destrucción; Z17 (leer después del `delete`) y Z18 (dos `delete`) se rechazan; BxTryAddFail, BxUpdate,
SortedListBox, DictGuardValue, DictHolderValue y las de `SortedList`, `SortedDictionary` y `PriorityQueue`
destruyen cada valor una vez.

Sin cambios en el resto (`seq.cmd f121`): `all.cmd` (96 y 30 casos, `awcheck` sólo cambia en `a4`, como
antes), `asynccases` 86, `gencases` 76 y `loopcases` 10 PASS; los 66 programas de Freeze30 (`d27\f30.cmd`) en
`-O0` y `-O2`; los 27 proyectos de `tests/cases` verifican; las fuentes de d28 pasan el check (MacroA ya fallaba
por un local C-01); las tres bibliotecas pasan el check.

Prueba nueva: `Freeze30ContractTests::testDeleteOwnedParameter` (F121Delete 33, F121DeleteThenRead y
F121DeleteTwice rechazadas, F121DictionaryTryAdd y F121SortedListTryAdd 22). Compila en `A:\temp\nsc-dec\bt`; se
ejecuta en el gate final.

## No verificado

- Batería completa y unitarias: gate final.
