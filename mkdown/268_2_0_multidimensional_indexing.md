# 268 — Indexado multidimensional / Multidimensional indexing

## Resumen / Summary

Los arreglos fijos multidimensionales admiten `array[i0, i1, ...]` para lectura, escritura,
dirección y movimiento explícito de un elemento. Cada índice consume una dimensión del tipo fijo y
usa el stride que ya calculó el layout canónico.

Fixed multidimensional arrays accept `array[i0, i1, ...]` for reads, writes, addresses, and explicit
element moves. Each index consumes one fixed type dimension and uses the stride already computed by
the canonical layout.

## Archivos y funciones / Files and functions

- `src/Sema/SemanticAnalyzer.cpp` — `analyzeIndex()` valida listas de índices, rango constante y
  tipo resultante en el frontend clásico.
- `src/HIR/HirBuilder.cpp` — `lowerAddress()` encadena un `IndexAddress` por dimensión.
- `src/Frontend/ProjectMethodBodyModel.cpp` — el binding OOP de `IndexExpression` consume el rank,
  conserva el tipo final y distingue fixed arrays de punteros e indexadores de usuario.
- `src/Frontend/ProjectOopHirModel.cpp` — construye una cadena de `RawElementAddress` con tipos de
  elemento intermedios.
- `tests/unit/MultidimensionalIndexTests.cpp` — certifica lecturas, escrituras, forma encadenada,
  strides row-major, límites constantes y exceso de rank.
- `tests/unit/SemanticTests.cpp`, `CMakeLists.txt` y `docs/1.0.1/` — cobertura clásica, registro y
  contrato público.

## Implementación y decisiones / Implementation and decisions

No se linealizan índices con una fórmula duplicada. `matrix[row, column]` genera dos direcciones:
la primera avanza por `sizeof(T[columns])` y la segunda por `sizeof(T)`. MIR obtiene ambos strides
desde `ProjectObjectLayoutModel`, la misma autoridad usada por campos, `sizeof`, LLVM y headers C.

Indices are not flattened with a duplicated formula. `matrix[row, column]` produces two addresses:
the first advances by `sizeof(T[columns])` and the second by `sizeof(T)`. MIR obtains both strides
from `ProjectObjectLayoutModel`, the same authority used by fields, `sizeof`, LLVM, and C headers.

La rama multidimensional se activa sólo para fixed arrays. Los punteros crudos y strings conservan
exactamente un índice; los indexadores declarados por el usuario conservan su lista normal de
argumentos y resolución de overload. Una lista parcial produce la submatriz restante como valor no
asignable, y la forma `matrix[i][j]` continúa siendo equivalente.

The multidimensional branch activates only for fixed arrays. Raw pointers and strings retain
exactly one index; user-declared indexers keep their ordinary argument list and overload resolution.
A partial list yields the remaining subarray as a non-assignable value, and `matrix[i][j]` remains
equivalent.

## Evidencia dirigida / Focused evidence

- `int[2,3]` usa stride 12 para la fila y stride 4 para el elemento.
- Lectura y escritura con coma llegan a MIR.
- La forma encadenada y la forma por rank compilan sobre el mismo almacenamiento.
- Un índice constante igual a la extensión y un índice adicional al rank se rechazan.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade indexado multidimensional por strides / Add stride-based multidimensional indexing`

**Descripción / Description**

`Permite array[i0, i1, ...] sobre arreglos fijos, encadena direcciones con el stride canónico de cada dimensión y valida tipos, rank y límites constantes. / Enables array[i0, i1, ...] on fixed arrays, chains addresses with each dimension's canonical stride, and validates types, rank, and constant bounds.`
