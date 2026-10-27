# Congelamiento 3.0, elemento 130 (F2) — un arreglo local sin `owned` no posee sus referencias

Fecha: 2026-10-26. Revisión de partida: `c86fbb8cd`.

## Nota de trabajo

```text
HEAD: c86fbb8cd
Elemento: 130 del plan de 146 (defecto F2, reproducido al verificar Dictionary y PriorityQueue, 121 y 125)
Contrato vigente: C-01; decisión de Alberto (2026-10-26) sobre F2, abajo
Documento histórico que podría contradecirlo: ninguno. DEC-06 (12) y DEC-11 (15) no cambian
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp
  src/Frontend/ProjectCallableResultMerger.cpp
  src/Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp
  src/Frontend/HIR/HirExceptionAndCleanup.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

`TryGetValue` y `TryPeek` escriben el resultado en un arreglo del llamador. La forma natural de usarlos
corrompía el heap con valores de clase:

```text
Box[] r = new Box[1];
d.TryGetValue(1, r);   // r[0] es el Box que sigue en el Dictionary
delete[] r;            // destruía ese Box; el Dictionary lo destruía otra vez
```

Un local `T[]` sin `owned` inicializado con `new` heredaba el ownership de almacenamiento del `new`. `delete[]`
lo trataba como dueño de sus elementos. El mismo arreglo obtenido de una llamada no lo era (sonda Z15).

| Sonda (`A:\temp\nsc-dec\d30`) | Forma | Correcto | Antes |
|---|---|---|---|
| Z11NewLocalAliasDelete | `r[0] = d[1]; delete[] r;` | 1 destrucción | heap corrupto |
| Z4ArrayParamStoreDelete | el elemento lo escribe un método por el parámetro `Box[]` | 1 | heap corrupto |
| BxTryGet | `Dictionary<int, Box>.TryGetValue` + `delete[]` | 4 de 4 | heap corrupto |
| PbTryPeek | `PriorityQueue<Box, int>.TryPeek` + `delete[]` | 4 de 4 | heap corrupto |
| Z15CallLocalAliasDelete | el mismo arreglo devuelto por una llamada | 1 | 1 |

## Decisión de Alberto (2026-10-26)

Un `T[]` sin `owned`/`raii` no es dueño de sus elementos:

- almacena referencias no dueñas;
- `delete[]` libera únicamente el bloque;
- `TryGetValue`, `TryPeek` y asignaciones como `r[i] = otro` pueden guardar aliases sin transferir ownership;
- para que un arreglo posea objetos debe declararse con ownership explícito y recibirlos por transferencia;
- DEC-11 no cambia para structs con destructor guardados por valor: sus slots vivos sí ejecutan destructor
  porque el valor forma parte del bloque.

## Contrato

`delete[] a`, con `a` un local declarado sin `owned`/`raii` (y que no es `ref`) cuyo tipo de elemento es una
clase o interfaz, libera sólo el bloque. El análisis marca el sitio (`arrayDeleteKeepsElements`) y el HIR baja
la desasignación sin ownership de elementos.

## SCOPE LOCK

- **Cambia.** Sólo `delete[]` de un local plano con elementos de clase o interfaz.
- **Idéntico.**
  - Locales `owned T[]` (DEC-06: `delete[]` consume y destruye lo recibido, Z19).
  - Arreglos de structs con destructor (DEC-11: los slots vivos se destruyen, Z20).
  - Arreglos de strings, campos (`owned` o no), parámetros y elementos de arreglos dentados.
  - El rechazo de `delete` de un local `raii`.

## Consecuencia observable y revisión de históricos

Un objeto nuevo guardado directamente en un arreglo plano (`r[0] = new Box(1); delete[] r;`) ya no se destruye
en el `delete[]` (sonda Z21): el arreglo no es su dueño. Esa escritura no se rechaza hoy. Rechazarla sería
extender C-01 a un destino nuevo (un elemento de arreglo no dueño), y eso no está autorizado.

Revisión focal del árbol (`A:\temp\nsc-dec\f2\scan.py`): 226 locales `T[]` planos creados con `new` y
borrados con `delete[]`. 208 son de tipos primitivos, strings o parámetros genéricos. De los 18 restantes, los
que reciben un objeto nuevo en un elemento son:

| Archivo | Arreglo | Efecto |
|---|---|---|
| `examples/project_stage52/Main.ns:63` | `Payload[] transferred` | ninguno (1) |
| `examples/tutorial_23_huffman_priority_queue/Main.ns:213` | `HuffmanNode[] pool` | fuga de los nodos (2) |
| `book_examples` 02, 19, 20, 25 | `Employee`, `Edge`, `Item`, `GcNode` | como `pool` (3) |

1. `AddRange` mueve el elemento y deja el slot en null antes del `delete[]`.
2. `delete[] pool` ya no libera los nodos. `HuffmanNode` no tiene destructor y la salida no cambia.
3. Fuera de esta revisión por instrucción de Alberto.

Los otros 12 guardan aliases (colas, pilas y nodos de árboles en `book_examples`, structs en `tests/cc1`,
`tutorial_06` y `project_stage41`). Con clases, antes se destruían en el `delete[]`; ahora no.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas d30 en `-O0` y `-O2`: Z4, Z11,
BxTryGet, PbTryPeek, DictBoxValue y PQBox destruyen cada valor una vez; Z13, Z15, Z19 y Z20 no cambian.

Sin cambios en el resto (`seq.cmd f130f2`): `all.cmd` idéntico al de `f121`, `asynccases` 86, `gencases` 76 y
`loopcases` 10 PASS; los 66 programas de Freeze30 en `-O0` y `-O2`; `tests/cases`, d28 y bibliotecas como antes.

Prueba nueva: `Freeze30ContractTests::testPlainArrayDelete` (F130TryGetValueArray 411, F130TryPeekArray 511,
F130OwnedArrayDestroys 611, F130StructArrayDrops 711). Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate
final.

## No verificado

- Batería completa y unitarias: gate final.
