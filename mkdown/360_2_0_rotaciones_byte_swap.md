# Ítem 360 — Rotaciones y byte swap / Rotate and byte-swap intrinsics

## Resultado / Result

`BitOperations` incorpora rotación izquierda/derecha y cambio de endianidad para `uint` y `ulong`,
bajando a `llvm.fshl`, `llvm.fshr` y `llvm.bswap`. / `BitOperations` now provides left/right rotation
and endian reversal for `uint` and `ulong`, lowering to `llvm.fshl`, `llvm.fshr`, and `llvm.bswap`.

## Pruebas / Tests

La prueba ejecuta las seis sobrecargas en O0/O2 con resultado 42 e inspecciona ambos anchos de IR. /
The test executes all six overloads at O0/O2 with result 42 and inspects both IR widths.
