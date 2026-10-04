# NS3-T11-F02 — Atributos definidos por usuario / User-defined attributes

## ES

NativeSharp 3 acepta definiciones `#[attribute Nombre targets(...) repeatable(...) retention(...)]`
y usos `#[Nombre(constantes...)]`. El paso comptime las reconoce antes del lexer y sustituye cada
directiva por espacios sin alterar offsets ni líneas. Sólo se admiten argumentos enteros, booleanos
y strings constantes. El modelo semántico enlaza cada uso al tipo, campo o método siguiente,
valida targets y repetición, y conserva el orden fuente.

La retención predeterminada `compiletime` no llega al binario. `retention(runtime)` es opt-in y
publica únicamente ese subconjunto en `runtimeAttributes`, listo para una sección nativa; no crea
reflection global. Los registros comptime viajan con la unidad, por lo que un productor puede
transportarlos junto con su metadata sin reinterpretar el texto del consumidor.

## EN

NativeSharp 3 accepts `#[attribute Name targets(...) repeatable(...) retention(...)]` definitions
and `#[Name(constants...)]` uses. The comptime pass recognizes them before lexing and blanks each
directive without changing source offsets or lines. Arguments are restricted to integer, boolean,
and string constants. The semantic model binds each use to the following type, field, or method,
validates target and repetition rules, and preserves source order.

The default `compiletime` retention never reaches the binary. `retention(runtime)` is opt-in and
publishes only that subset through `runtimeAttributes`, ready for a native section without a global
reflection registry. Comptime records travel with their source unit so producers can transport the
metadata without reparsing consumer text.

## Evidencia / Evidence

`nsc_ns3_t11_f02_user_attributes_tests` covers productive parsing, constant arguments, semantic
attachment, runtime stripping/opt-in, invalid targets, duplicates, and non-constant rejection.
