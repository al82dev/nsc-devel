# 379 — Visualización SIMD y matrices / SIMD and matrix debugging

## Español

El backend CodeView reconoce los vectores públicos de `NativeSharp.Core.Numerics` y conserva sus
campos escalares `X/Y/Z/W`, pero añade también una vista superpuesta artificial `Lanes` con el tipo
vectorial nativo de LLVM. Visual Studio y WinDbg pueden así inspeccionar cada carril y relacionar el
valor con registros SIMD sin sacrificar los nombres que ve el programa fuente.

Las matrices mantienen su estructura fuente por columnas (`C0` a `C3`). Cada columna reutiliza el
tipo vectorial enriquecido, de modo que el árbol de variables permite navegar matriz, columna y
carril sin inventar una disposición distinta de la ABI real.

El corpus O0 construye `NativeSharp.Core` como biblioteca precompilada, la consume desde una
biblioteca productora y enlaza un ejecutable consumidor. La prueba ejecuta el programa y examina el
PDB final para exigir `Float32x4`, la vista `Lanes`, la columna `C3` y los locales `simdVector` y
`simdMatrix`. Esto cubre también la conservación multinódulo del metadato de depuración.

## English

The CodeView backend recognizes the public vectors in `NativeSharp.Core.Numerics` and preserves
their scalar `X/Y/Z/W` fields while also adding an artificial overlapping `Lanes` view backed by an
LLVM native vector type. Visual Studio and WinDbg can therefore inspect individual lanes and relate
the value to SIMD registers without losing the names visible in source code.

Matrices retain their source column structure (`C0` through `C3`). Every column reuses the enriched
vector type, so the variables tree can navigate matrix, column, and lane without inventing a layout
different from the real ABI.

The O0 corpus builds `NativeSharp.Core` as a precompiled library, consumes it from a producer
library, and links a consumer executable. The test runs the program and inspects the final PDB for
`Float32x4`, the `Lanes` view, column `C3`, and the `simdVector` and `simdMatrix` locals. This also
covers cross-module preservation of the debug metadata.
