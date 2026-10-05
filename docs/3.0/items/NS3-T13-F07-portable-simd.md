# NS3-T13-F07 — SIMD portable / Portable SIMD

## Español

NativeSharp 3 incorpora los tipos estructurales `simd<T, N>` y `mask<T, N>`. El conjunto de
lanes es explícito (enteros de 8/16/32/64 bits, `float` y `double`), el número de lanes es una
constante potencia de dos y el ancho total no supera 512 bits. La identidad canónica conserva
tipo y cantidad, por lo que módulos, layout y LLVM observan el mismo tipo.

Las operaciones aritméticas requieren formas idénticas; las comparaciones producen una máscara
de la misma forma y las máscaras admiten `&`, `|` y `^`. Una máscara verdadera es todos-los-bits
y una falsa es cero. Los shuffles se validan como listas constantes completas y dentro de rango.
El backend emite vectores LLVM; la scalarización futura puede sustituir esa selección sin cambiar
resultados, incluidos NaN y el modo de punto flotante activo.

La prueba dirigida compila y ejecuta a `-O0` con NSLink, además de fijar identidad, lanes, máscaras
y shuffles. No se ejecutó baseline ni una suite general.

## English

NativeSharp 3 adds structural `simd<T, N>` and `mask<T, N>` types. Lane types are explicit
(8/16/32/64-bit integers, `float`, and `double`), lane count is a power-of-two constant, and total
width is limited to 512 bits. Canonical identity retains both lane type and count so module
boundaries, layout, and LLVM agree on the same type.

Arithmetic requires identical shapes; comparisons produce a mask of the same shape, and masks
support `&`, `|`, and `^`. True mask lanes are all-bits-set and false lanes are zero. Shuffles are
validated as complete, in-range constant lists. The backend emits LLVM vectors; a future scalar
fallback may replace target selection without changing observable results, including NaN and the
active floating-point mode.

The directed test compiles and runs at `-O0` through NSLink and locks identity, lanes, masks, and
shuffle rules. No baseline or broad suite was executed.
