# Ítem 362 — Contador de tiempo de CPU / CPU timestamp counter

## Resultado / Result

`HardwareIntrinsics.ReadTimestamp()` devuelve un contador de ciclos de 64 bits mediante
`llvm.readcyclecounter`, sin pasar por el reloj del sistema operativo. / `ReadTimestamp()` returns a
64-bit cycle counter through `llvm.readcyclecounter`, without using an operating-system clock.

## Pruebas / Tests

La prueba toma dos lecturas separadas por `Pause`, exige orden no decreciente en O0/O2 y comprueba
el intrínseco en IR. / The test takes two readings separated by `Pause`, requires nondecreasing order
at O0/O2, and checks the intrinsic in IR.
