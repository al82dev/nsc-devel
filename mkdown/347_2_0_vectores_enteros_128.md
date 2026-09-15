# Ítem 347 — Vectores enteros de 128 bits / 128-bit integer vectors

## Resultado / Result

`NativeSharp.Core.Numerics` publica `Int32x4` e `Int64x2` como carriers secuenciales estables de
16 bytes. Sus operaciones exactas `Add`, `Subtract` y `Multiply` cruzan HIR y MIR como intrínsecos
y se ejecutan en LLVM como `<4 x i32>` o `<2 x i64>`. / `NativeSharp.Core.Numerics` exposes
`Int32x4` and `Int64x2` as stable 16-byte sequential carriers. Their exact `Add`, `Subtract`, and
`Multiply` operations cross HIR and MIR as intrinsics and execute in LLVM as `<4 x i32>` or
`<2 x i64>`.

## Contrato de representación / Representation contract

La representación pública sigue siendo un struct ordinario, apto para metadata, PDB y ABI. El
backend convierte el carrier a un vector únicamente alrededor de la operación y vuelve a convertir
el resultado; la aritmética entera conserva semántica modular por carril. El reconocimiento exige
identidad completa y firma `vector, vector -> vector`, por lo que un método homónimo del usuario no
se vuelve intrínseco. / The public representation remains an ordinary struct suitable for metadata,
PDB, and ABI. The backend converts the carrier to a vector only around the operation and converts
the result back; integer arithmetic keeps per-lane modular semantics. Recognition requires full
identity and a `vector, vector -> vector` signature, so a same-named user method never becomes an
intrinsic.

## Pruebas / Tests

La prueba focalizada ejecuta ambas anchuras en O0 y O2 con resultado 42 e inspecciona el IR para
exigir `add <4 x i32>`, `mul <2 x i64>` y el puente de retorno al carrier. / The focused test runs
both lane widths at O0 and O2 with result 42 and inspects IR for `add <4 x i32>`, `mul <2 x i64>`,
and the return bridge back to the carrier.
