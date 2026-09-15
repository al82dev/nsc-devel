# Ítem 350 — Loads/stores SIMD alineados / Aligned SIMD loads and stores

## Resultado / Result

`Int32x4.LoadAligned` y `StoreAligned` convierten un puntero a carrier en una carga o escritura
LLVM `<4 x i32>` única con `align 16`. La alineación es una precondición explícita del llamador, no
una comprobación silenciosa del runtime. / `Int32x4.LoadAligned` and `StoreAligned` turn a carrier
pointer into a single LLVM `<4 x i32>` load or store with `align 16`. Alignment is an explicit
caller precondition, not a silent runtime check.

## Frontera estable / Stable boundary

La memoria visible conserva el struct secuencial; solamente la operación interna usa el tipo vector
LLVM. Los puentes temporales también publican alineación 16, evitando que O0 dependa por accidente
del layout de stack. / Visible memory remains the sequential struct; only the internal operation
uses the LLVM vector type. Temporary bridges also publish 16-byte alignment, preventing O0 from
accidentally depending on stack layout.

## Pruebas / Tests

La prueba focalizada hace ida y vuelta por punteros en O0/O2 con resultado 42 y exige en IR una
carga, una escritura vectorial y `align 16`. / The focused test round-trips through pointers at
O0/O2 with result 42 and requires a vector load, vector store, and `align 16` in IR.
