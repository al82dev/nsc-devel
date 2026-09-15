# Ítem 349 — Vectores enteros de 256 bits / 256-bit integer vectors

## Resultado / Result

`Int32x8` e `Int64x4` completan la familia entera firmada de 256 bits. Sus operaciones exactas
`Add`, `Subtract` y `Multiply` bajan como `<8 x i32>` y `<4 x i64>`, con aritmética modular por
carril. / `Int32x8` and `Int64x4` complete the signed 256-bit integer family. Their exact `Add`,
`Subtract`, and `Multiply` operations lower as `<8 x i32>` and `<4 x i64>`, with modular per-lane
arithmetic.

## ABI y alineación / ABI and alignment

Los tipos públicos conservan structs secuenciales de 32 bytes. Los puentes LLVM usan alineación
explícita de 32 bytes y devuelven el resultado al carrier, manteniendo estable metadata, PDB y ABI.
/ Public types remain sequential 32-byte structs. LLVM bridges use explicit 32-byte alignment and
return the result to the carrier, keeping metadata, PDB, and ABI stable.

## Pruebas / Tests

La prueba focalizada ejecuta ambos tipos en O0/O2 con resultado 42 y exige `add <8 x i32>`, `mul <4
x i64>` y el puente de salida en IR. / The focused test executes both types at O0/O2 with result 42
and requires `add <8 x i32>`, `mul <4 x i64>`, and the output bridge in IR.
