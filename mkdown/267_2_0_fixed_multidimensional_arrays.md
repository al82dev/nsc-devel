# 267 — Arreglos multidimensionales fijos / Fixed multidimensional arrays

## Resumen / Summary

NativeSharp acepta ahora `T[d0, d1, ...]` como tipo de arreglo fijo multidimensional. El valor
conserva almacenamiento inline, contiguo y row-major; no crea un objeto administrado, no contiene
cabecera y no realiza asignaciones de heap.

NativeSharp now accepts `T[d0, d1, ...]` as a fixed multidimensional array type. The value retains
inline, contiguous, row-major storage; it does not create a managed object, carry a header, or
perform heap allocation.

## Archivos y funciones / Files and functions

- `src/Parser/Parser.cpp` — `Parser::parseType()` lee todas las dimensiones de un rank fijo y las
  normaliza a modificadores anidados.
- `src/Sema/SemanticAnalyzer.cpp` — `SemanticAnalyzer::resolveType()` permite que un fixed array
  completo sea el elemento de la siguiente dimensión y comprueba cada tamaño.
- `tests/unit/FixedMultidimensionalArrayTests.cpp` — certifica identidad canónica, layout, structs
  elemento, tres dimensiones y rechazos por dimensión cero o ausente.
- `tests/unit/SemanticTests.cpp` — actualiza el contrato del frontend clásico.
- `CMakeLists.txt` — registra la prueba enfocada `nsc.language.fixed-multidimensional-arrays`.
- `docs/1.0.1/` — actualiza gramática, especificación, libro, changelog, conformidad y roadmap.

## Implementación y decisiones / Implementation and decisions

No se agregó un segundo tipo interno para matrices. `int[3, 4]` se normaliza a la identidad
canónica `builtin:int[4][3]`: el sufijo exterior representa tres filas y cada fila contiene cuatro
enteros contiguos. LLVM obtiene naturalmente `[3 x [4 x i32]]`, mientras que layout, `sizeof`,
especializaciones genéricas y ABI siguen usando las funciones existentes para fixed arrays.

No second internal matrix type was added. `int[3, 4]` normalizes to the canonical identity
`builtin:int[4][3]`: the outer suffix represents three rows and each row contains four contiguous
integers. LLVM naturally obtains `[3 x [4 x i32]]`, while layout, `sizeof`, generic specialization,
and ABI continue using the existing fixed-array services.

Las dimensiones se validan individualmente como constantes enteras positivas. El cálculo de
tamaño permanece recursivo y comprobado contra overflow por `TargetDataLayout::layoutArray()`.
El indexado `array[i, j]` pertenece deliberadamente al item 268; este commit entrega el tipo, su
identidad y su representación física.

Each dimension is independently validated as a positive integer constant. Size calculation stays
recursive and overflow-checked by `TargetDataLayout::layoutArray()`. `array[i, j]` indexing is
deliberately item 268; this commit delivers the type, identity, and physical representation.

## Evidencia dirigida / Focused evidence

- Parser y semántica aceptan `T[3,4]` y la forma anidada compatible.
- `int[3,4]` produce 48 bytes inline, alineación 4 y desplaza el campo siguiente al byte 48.
- `Cell[2,3,4]` compone correctamente tres niveles y `sizeof` observa 192 bytes.
- `T[3,0]` y `T[3,]` se rechazan durante compilación.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade arreglos multidimensionales fijos / Add fixed multidimensional arrays`

**Descripción / Description**

`Incorpora T[d0, d1, ...] como almacenamiento inline row-major, lo normaliza al modelo canónico de arreglos anidados y valida cada dimensión constante. / Adds T[d0, d1, ...] as row-major inline storage, normalizes it to the canonical nested-array model, and validates every constant dimension.`
