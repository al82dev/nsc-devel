# Ítem 352 — `float2` y `float3` / Compact float vectors

## Resultado / Result

`Float32x2` y `Float32x3` son carriers compactos de 8 y 12 bytes, sin padding público añadido.
`Add`, `Subtract` y `Multiply` bajan a vectores LLVM de dos y tres carriles y vuelven al struct
secuencial. / `Float32x2` and `Float32x3` are compact 8- and 12-byte carriers with no added public
padding. `Add`, `Subtract`, and `Multiply` lower to two- and three-lane LLVM vectors and return to
the sequential struct.

## Pruebas / Tests

La prueba ejecuta ambos tipos en O0/O2 con resultado 42 y exige `fadd <2 x float>` y `fmul <3 x
float>` en el IR. Así certifica simultáneamente semántica, anchura física y ausencia de promoción
automática a `float4`. / The test executes both types at O0/O2 with result 42 and requires `fadd <2
x float>` and `fmul <3 x float>` in IR, certifying semantics, physical width, and no automatic
promotion to `float4`.
