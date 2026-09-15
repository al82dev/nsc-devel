# Ítem 359 — Popcount, CLZ y CTZ / Bit-count intrinsics

## Resultado / Result

`BitOperations` ofrece conteo de bits, ceros iniciales y ceros finales para `uint` y `ulong`. Las
seis sobrecargas bajan a `llvm.ctpop`, `llvm.ctlz` y `llvm.cttz`; CLZ/CTZ definen el caso cero. /
`BitOperations` provides population, leading-zero, and trailing-zero counts for `uint` and `ulong`.
All six overloads lower to the matching LLVM intrinsics; CLZ/CTZ define the zero case.

## Pruebas / Tests

La prueba ejecuta los dos anchos en O0/O2 con resultado 42 e inspecciona los tres intrínsecos. /
The test executes both widths at O0/O2 with result 42 and inspects all three intrinsics.
