# NS3-T14-F01 — Literales numéricos tipados / Typed numeric literals

## ES

Un literal entero se analiza primero como magnitud sin tipo y sin signo. La negación es una expresión unaria posterior, lo que permite representar correctamente el mínimo de cada tipo firmado. Se aceptan bases binaria, octal, decimal y hexadecimal, separadores únicamente entre dígitos y los sufijos `i8/i16/i32/i64/isize/u8/u16/u32/u64/usize`.

La resolución elige primero el sufijo, después un tipo contextual compatible y, sin ambos, `i32`, `i64` o `u64` en ese orden. `isize/usize` quedan congelados a 64 bits por el objetivo Windows x64. El overflow se diagnostica sobre la magnitud y el signo antes de cualquier conversión, ranking de overload o uso como const generic.

Prueba dirigida: `nsc_ns3_t14_numeric_literal_tests` cubre bases, separadores, sufijos, límites, unary minus, resolución contextual, ancho nativo y diagnósticos causales.

## EN

An integer literal is first parsed as an untyped, unsigned magnitude. Negation is a later unary expression, which makes every signed minimum representable. Binary, octal, decimal, and hexadecimal bases are accepted, separators are allowed only between digits, and the full `i8/i16/i32/i64/isize/u8/u16/u32/u64/usize` suffix family is recognized.

Resolution selects an explicit suffix first, then a compatible contextual type, and otherwise `i32`, `i64`, or `u64` in that order. `isize/usize` are frozen to 64 bits by the Windows x64 target. Overflow is diagnosed from magnitude and sign before any conversion, overload ranking, or const-generic use.

Focused test: `nsc_ns3_t14_numeric_literal_tests` covers bases, separators, suffixes, boundaries, unary minus, contextual resolution, native width, and causal diagnostics.
