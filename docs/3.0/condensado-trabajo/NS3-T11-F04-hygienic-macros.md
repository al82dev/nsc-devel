# NS3-T11-F04 — Macros sintácticas higiénicas / Hygienic syntax macros

## ES

La superficie `#macro nombre(parámetros) { token-tree }` y `#expand nombre(argumentos)` se expande
antes del lexer mediante `MacroExpansionEngine`. `$parametro` inserta el árbol de tokens del sitio
de llamada; `$#nombre` declara una identidad higiénica dentro del namespace reservado
`__nsc_macro_<scope>_`. El fuente del usuario no puede declarar ese prefijo, por lo que un nombre
introducido nunca captura ni es capturado por uno escrito por el usuario. Cada expansión conserva
nombre, span de invocación, longitud generada y scope en el registro comptime.

Los argumentos balancean paréntesis, corchetes, llaves y strings. Las expansiones anidadas se
resuelven de forma determinista y el límite evita recursión infinita. El código generado vuelve a
entrar al lexer, parser y análisis de cuerpos ordinarios, por lo que privacidad de módulos y tipos
se validan igual que en código escrito a mano.

## EN

The `#macro name(parameters) { token-tree }` and `#expand name(arguments)` surface is expanded
before lexing by `MacroExpansionEngine`. `$parameter` inserts the caller token tree; `$#name`
introduces a hygienic identity in the reserved `__nsc_macro_<scope>_` namespace. User source cannot
declare that prefix, so generated names cannot capture, or be captured by, user bindings. Each
expansion records its macro name, invocation span, generated length, and scope in comptime data.

Arguments balance parentheses, brackets, braces, and strings. Nested expansion is deterministic
and bounded against infinite recursion. Generated source re-enters the ordinary lexer, parser, and
method-body analysis, so module privacy and typing are checked exactly like handwritten code.

## Evidencia / Evidence

`nsc_ns3_t11_f04_hygienic_macros_tests` compiles nested productive expansions, checks capture
avoidance and trace records, rejects namespace forgery, and proves deterministic recursion limits.
