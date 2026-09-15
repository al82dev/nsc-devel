# Ítem 351 — Loads/stores SIMD no alineados / Unaligned SIMD loads and stores

## Resultado / Result

`Int32x4.LoadUnaligned` y `StoreUnaligned` emiten cargas y escrituras LLVM `<4 x i32>` con
`align 1`. No afirman una alineación que el puntero no garantiza y siguen moviendo los 128 bits en
una sola operación vectorial. / `Int32x4.LoadUnaligned` and `StoreUnaligned` emit LLVM `<4 x i32>`
loads and stores with `align 1`. They do not claim alignment the pointer cannot guarantee and still
move all 128 bits in one vector operation.

## Prueba adversarial / Adversarial test

La prueba usa deliberadamente `storage + 1`, hace store/load y ejecuta el resultado 42 en O0/O2.
Además exige `align 1` en IR, separando de forma observable esta API de la variante alineada. / The
test deliberately uses `storage + 1`, round-trips through store/load, and executes result 42 at
O0/O2. It also requires `align 1` in IR, observably separating this API from the aligned variant.
