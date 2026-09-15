# Ítem 348 — Vectores float/double de 256 bits / 256-bit floating vectors

## Resultado / Result

`Float32x8` y `Float64x4` añaden carriers públicos secuenciales de 32 bytes y operaciones
intrínsecas `Add`, `Subtract` y `Multiply`. El backend emite vectores LLVM `<8 x float>` y
`<4 x double>` sin exponer tipos dependientes del target en metadata o PDB. / `Float32x8` and
`Float64x4` add public sequential 32-byte carriers and intrinsic `Add`, `Subtract`, and `Multiply`
operations. The backend emits LLVM `<8 x float>` and `<4 x double>` vectors without exposing
target-dependent types in metadata or PDB.

## Alineación y portabilidad / Alignment and portability

Los puentes internos reservan y acceden sus carriers con alineación explícita de 32 bytes. LLVM
puede legalizar el vector según las features del target seleccionado; el contrato del lenguaje es
la operación por carril y no una instrucción x86 concreta. / Internal bridges allocate and access
their carriers with explicit 32-byte alignment. LLVM may legalize the vector according to the
selected target features; the language contract is the per-lane operation rather than a specific
x86 instruction.

## Pruebas / Tests

La prueba focalizada ejecuta ambas precisiones en O0/O2 con resultado 42 e inspecciona `fadd <8 x
float>`, `fmul <4 x double>` y el puente de carrier. / The focused test executes both precisions at
O0/O2 with result 42 and inspects `fadd <8 x float>`, `fmul <4 x double>`, and the carrier bridge.
