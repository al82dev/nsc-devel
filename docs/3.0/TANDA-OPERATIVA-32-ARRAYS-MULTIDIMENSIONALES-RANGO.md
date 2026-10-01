# Tanda operativa 32 — arrays multidimensionales de rango

## Resultado

NativeSharp 3.0 incorpora arrays rectangulares nativos con identidad propia. `T[,]` y `T[,,]`
ya no se interpretan como arrays jagged: el rango sobrevive desde la sintaxis hasta CodeView,
NSMETA y el backend. La construcción `new T[d0, d1, ...]` reserva una sola región row-major,
comprueba cada dimensión y conserva dimensiones y strides en una cabecera ABI estable para
Windows x64.

## Contrato de lenguaje

- `T[,]`, `T[,,]` y rangos superiores usan un único modificador `RangeArray`.
- La identidad canónica conserva las comas: `builtin:int[,]` es distinta de
  `builtin:int[][]`.
- Una construcción rectangular contiene exactamente una expresión integral por dimensión.
- La indexación exige exactamente el rango declarado y una expresión integral no booleana por
  dimensión.
- `.Length` devuelve el producto total de dimensiones.
- El orden lineal es row-major: el último índice varía más rápido.
- Un acceso comprueba por separado `0 <= índice < dimensión` antes de formar el offset.
- La memoria de elementos se inicializa a cero. Por ello un array de referencias parcialmente
  poblado contiene `null` en las ranuras restantes y su cleanup puede recorrer la longitud total
  sin leer basura ni destruir owners inexistentes.

## ABI Windows x64

Para rango `R`, la asignación es una sola región:

```text
offset 0                     totalLength : u64
offset 8 .. 8*R              dimensions[R] : u64
offset 8*(1+R) .. 8*(1+2R)   strides[R] : u64
offset align(8*(1+2R), A)    data[totalLength] : T
```

El rango no se duplica en memoria: pertenece al tipo canónico. La construcción evalúa dimensiones
una sola vez y en orden fuente. Cada multiplicación se limita antes de ejecutarse; también se
comprueba que cabecera, alineación y datos sean representables. Los strides se producen de derecha
a izquierda y la indexación usa `sum(index[i] * stride[i])`.

La destrucción reutiliza `totalLength`, selecciona el offset de datos a partir del rango del tipo,
limpia cada slot owning antes de descender y libera la región exterior al final. Esto cubre arrays
de clases, strings, delegates y arrays anidados, incluso cuando sólo algunas celdas se poblaron.

## Representación intermedia y herramientas

- HIR: `range-array-allocation`, `range-array-element-value` y
  `range-array-element-address`, con `nativeArrayRank` explícito.
- MIR: operaciones equivalentes y schema `1.21`; el dump publica rango, tipo de elemento,
  tamaño y alineación.
- LLVM: construcción de cabecera, overflow checks, bounds checks por dimensión y cálculo row-major.
- NSMETA: las firmas conservan literalmente `[,]`, por lo que un consumidor precompilado no las
  confunde con `[][]`.
- AST/LSP: `TypeModifierSyntax` retiene `arrayRank`; el spelling y los clones genéricos conservan
  el rango.
- CodeView: el tipo conserva su nombre rectangular y publica `Length`, `DimensionN`, `StrideN` y
  `Data` con offsets físicos correctos.
- La identidad de caché MIR incluye el rango; cambiarlo invalida el artefacto.

## Archivos modificados

- `include/nativesharp/AST/TypeSyntax.h`: modificador rectangular y rango.
- `include/nativesharp/Frontend/ProjectTypeConversion.h` y
  `src/Frontend/ProjectTypeConversion.cpp`: reconocimiento, rango y elemento canónicos.
- `src/Parser/Parser.cpp`: gramática de tipos y construcciones rectangulares.
- `src/AST/AstDumper.cpp`, `src/Frontend/ProjectGenericConstraintModel.cpp` y
  `src/Frontend/ProjectGenericModel.cpp`: spelling, constraints, clonación e identidad genérica.
- `src/Frontend/ProjectMemberModel.cpp` y `src/Frontend/ProjectObjectLayoutModel.cpp`: clave
  canónica y layout de referencia.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectMethodBodyModel.cpp`: validación y bindings semánticos.
- `include/nativesharp/Frontend/ProjectOopHirModel.h`,
  `src/Frontend/ProjectOopHirModel.cpp` y su dumper: operaciones HIR y rango.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`,
  `src/Frontend/ProjectOopMirModel.cpp` y su dumper: schema/operaciones MIR.
- `src/Frontend/ProjectCostExplorer.cpp`: coste de heap rectangular.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: ABI, construcción, acceso, cleanup y CodeView.
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`: rango en la clave de caché.
- `CMakeLists.txt`: target dirigido T32.

## Archivo nuevo

- `tests/unit/NativeSharp3T32RangeArrayTests.cpp`: prueba vertical aislada. Es nueva porque T32
  necesita observar conjuntamente parser, ejecución, metadata, dumps, depuración y fallos de
  rango sin incorporar esta carga al baseline durante desarrollo.

## Validación dirigida

- Construcción de `nsc` Debug/O0: correcta.
- `nsc_ns3_t32_range_array_tests`: 10/10.
- La prueba cubre productor `.lib/.nsmeta`, consumidor por `LibraryReference`, ranks 2 y 3,
  `.Length`, escritura/lectura row-major, paso por función precompilada, array owning parcialmente
  poblado, destrucción exacta, rechazo de rango incorrecto, trap de bounds, dumps HIR/MIR y objeto
  CodeView.
- No se ejecutó CTest completo ni baseline.

## Mejora obtenida

El lenguaje gana matrices y tensores compactos sin el coste ni la fragmentación de un jagged
array. El contrato queda observable y estable en todos los límites que importan: tipos, metadata,
ABI, cache, depuración y ejecución. Un cambio futuro de rango o layout ya no puede pasar de forma
silenciosa como si fuera `T[][]`.
