# 385 — Núcleo `comptime/#run` / Compile-time execution core

NativeSharp 2.0 incorpora una primera ruta ejecutable de `comptime`. `comptime` es un marcador
contextual que puede preceder una declaración y `#run(expresión)` evalúa una expresión entera con
semántica `int64` antes del lexer y el parser ordinarios. / NativeSharp 2.0 now has its first
executable comptime path. `comptime` is a contextual declaration marker and
`#run(expression)` evaluates an `int64` integer expression before the ordinary lexer and parser.

La sustitución conserva exactamente el número de bytes de la fuente original. Así, los rangos,
líneas y columnas de todos los diagnósticos posteriores siguen apuntando al texto escrito por el
usuario. Comentarios, strings y caracteres nunca se interpretan como directivas. / Replacement
retains the exact original byte count, so all later diagnostic ranges, lines, and columns still
refer to user-authored text. Comments, strings, and characters are never interpreted as directives.

El núcleo admite paréntesis, signo, complemento, suma, resta, multiplicación, división, módulo,
shifts y operaciones bitwise. División por cero, overflow con signo, literales fuera de rango,
shifts inválidos y sintaxis incompleta son errores deterministas de compilación. No se carga ni se
ejecuta código nativo. / The core supports parentheses, sign, complement, arithmetic, shifts, and
bitwise operations. Division by zero, signed overflow, out-of-range literals, invalid shifts, and
incomplete syntax are deterministic compile-time errors. No native code is loaded or executed.
