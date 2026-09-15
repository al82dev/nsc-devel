# Ítem 361 — Prefetch y pause / Prefetch and pause intrinsics

## Resultado / Result

`HardwareIntrinsics.PrefetchRead` emite una precarga de datos de lectura con localidad alta y
`Pause` emite la espera cooperativa x64. Ambos son intrínsecos sin dependencia de runtime. /
`HardwareIntrinsics.PrefetchRead` emits a high-locality read data prefetch and `Pause` emits the x64
cooperative wait instruction. Both are runtime-free intrinsics.

## Pruebas / Tests

La prueba ejecuta ambos en O0/O2 con resultado 42 y exige `llvm.prefetch.p0` y
`llvm.x86.sse2.pause` en IR. / The test executes both at O0/O2 with result 42 and requires both LLVM
intrinsics in IR.
