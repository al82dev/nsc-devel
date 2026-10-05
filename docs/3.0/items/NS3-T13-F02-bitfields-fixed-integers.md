# NS3-T13-F02 — Bitfields y enteros de ancho fijo / Bit-fields and fixed-width integers

## Español

NativeSharp 3.0 normaliza `i8/u8`, `i16/u16`, `i32/u32`, `i64/u64`, `i128/u128` e
`isize/usize`. Los aliases de hasta 64 bits conservan las identidades históricas
`sbyte/byte/short/ushort/int/uint/long/ulong/nint/nuint`; `i128` y `u128` son carriers LLVM i128
de 16 bytes, alineados a 16 bytes en Windows x64. Las promociones preservan ancho y signo; una
conversión implícita sólo ensancha el rango.

Los bitfields de edición 3 exigen layout explícito `repr(C)` o `repr(native)`. El primer campo
ocupa los bits menos significativos de su unidad Win64 y los siguientes avanzan hacia los más
significativos. El tipo base define signedness y extensión de signo. `volatile` carga y escribe la
unidad completa sin atribuir atomicidad u orden entre hilos. Un ancho cero sólo es válido para un
campo anónimo y un ancho nunca puede exceder el carrier. `repr(transparent)` no admite unidades de
bitfields porque éstas no son el ABI transparente de un campo escalar ordinario.

La prueba dirigida compila y ejecuta O0 mediante NSLink, verifica tamaños 1…16, sign extension,
actualizaciones volatile y diagnósticos de layout/ancho. No ejecuta baseline.

## English

NativeSharp 3.0 normalizes `i8/u8`, `i16/u16`, `i32/u32`, `i64/u64`, `i128/u128`, and
`isize/usize`. Aliases up to 64 bits retain the historical
`sbyte/byte/short/ushort/int/uint/long/ulong/nint/nuint` identities. `i128` and `u128` are real
LLVM i128 carriers with 16-byte size and alignment on Windows x64. Promotions preserve width and
signedness; an implicit conversion may only widen the represented range.

Edition-3 bit-fields require explicit `repr(C)` or `repr(native)` layout. The first field occupies
the least-significant bits of its Win64 allocation unit and following fields advance toward the
most-significant bits. The base type defines signedness and sign extension. `volatile` reads and
writes the complete unit without inventing atomicity or inter-thread ordering. Zero width is
reserved for unnamed fields and width never exceeds the carrier. `repr(transparent)` cannot hide
a bit-field allocation unit because it is not the transparent ABI of an ordinary scalar field.

The directed test compiles and executes O0 through NSLink, checking sizes 1…16, sign extension,
volatile updates, and deterministic layout/width diagnostics. It does not run a baseline.
