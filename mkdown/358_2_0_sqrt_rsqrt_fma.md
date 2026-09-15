# Ítem 358 — Sqrt, rsqrt y FMA / Vector sqrt, rsqrt, and FMA

## Resultado / Result

`Float32x4` incorpora raíz, raíz recíproca y multiplicación-suma fusionada. La raíz y FMA bajan a
intrínsecos LLVM vectoriales; rsqrt conserva semántica exacta como `1 / sqrt`. / `Float32x4` now
provides square root, reciprocal square root, and fused multiply-add. Root and FMA lower to LLVM
vector intrinsics; rsqrt preserves exact semantics as `1 / sqrt`.

## Pruebas / Tests

La prueba ejecuta las tres operaciones en O0/O2 con resultado 42 y exige los intrínsecos
`llvm.sqrt.v4f32` y `llvm.fma.v4f32` en IR. / The test executes all three operations at O0/O2 with
result 42 and requires both LLVM intrinsics in IR.
