# Ítem 354 — Tipo `half` / Half-precision type

## Resultado / Result

`NativeSharp.Core.Numerics.Half` es un carrier IEEE 754 binary16 de dos bytes. `FromFloat` y
`ToFloat` son intrínsecos de firma exacta que bajan a `fptrunc` y `fpext`, conservando el dato a 16
bits entre conversiones. / `NativeSharp.Core.Numerics.Half` is a two-byte IEEE 754 binary16 carrier.
Exact-signature `FromFloat` and `ToFloat` intrinsics lower to `fptrunc` and `fpext`, keeping the value
at 16 bits between conversions.

## Pruebas / Tests

La prueba fija `sizeof(Half) == 2`, ejecuta el round-trip en O0/O2 con resultado 42 e inspecciona
ambas instrucciones LLVM. / The test pins `sizeof(Half) == 2`, executes the round trip at O0/O2 with
result 42, and inspects both LLVM instructions.
