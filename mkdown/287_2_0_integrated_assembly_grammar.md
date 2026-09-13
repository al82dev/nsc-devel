# 287 — Gramática de ensamblador integrado / Integrated assembly grammar

## Resultado / Result

NativeSharp reconoce ahora `asm { ... }` como una sentencia estructural dentro de una función o
método. El parser crea un nodo `InlineAssemblyStatement`, consume el bloque completo sin exigir
punto y coma, conserva una secuencia canónica provisional de sus tokens y continúa correctamente
con la siguiente sentencia. El mismo bloque en namespace produce un diagnóstico directo. /
NativeSharp now recognizes `asm { ... }` as a structural statement inside a function or method.
The parser creates an `InlineAssemblyStatement` node, consumes the complete block without requiring
a semicolon, retains a provisional canonical token sequence, and resumes at the following
statement. The same block at namespace scope produces a direct diagnostic.

## Archivos y funciones / Files and functions

- `include/nativesharp/AST/AstNodeKind.h` y `src/AST/AstNodeKind.cpp`
  - Agregan y nombran `InlineAssemblyStatement` como identidad sintáctica propia.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`
  - `parseStatement()` reconoce el inicio contextual `asm {`.
  - `parseInlineAssemblyStatement()` delimita braces balanceados, conserva el spelling canónico y
    diagnostica un bloque sin cierre.
  - `parseGlobalDeclaration()` rechaza el bloque fuera de un cuerpo y recupera la declaración
    posterior.
- `include/nativesharp/Diagnostics/DiagnosticId.h`, `src/Sema/SemanticAnalyzer.cpp` y
  `src/Frontend/ProjectMethodBodyModel.cpp`
  - Rechazan explícitamente la sentencia antes de HIR mientras el lowering no exista, evitando un
    fallo interno o un no-op silencioso.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp` y `CMakeLists.txt`
  - Certifican bloque no vacío, continuación sin semicolon, braces internos balanceados, uso de
    `asm` como identificador ordinario, rechazo en namespace y cierre faltante.
- `docs/1.0.1`
  - Actualiza gramática EBNF, especificación, contrato del compilador, mapas de conformidad,
    estado, changelog y roadmap sin anunciar todavía ejecución de asm.

## Implementación y decisiones / Implementation and design decisions

`asm` permanece contextual en esta entrega y sólo adquiere significado especial cuando le sigue
`{`. Esto evita convertir prematuramente en palabra reservada un identificador previamente legal.
El cuerpo se consume usando los tokens ordinarios y se une con espacios de forma determinista.
Esa cadena no pretende conservar formato, líneas ni columnas por instrucción: el modo léxico y las
ubicaciones exactas pertenecen expresamente al ítem 288. / `asm` remains contextual in this delivery
and only acquires special meaning when followed by `{`. This avoids prematurely turning a formerly
legal identifier into a reserved word. The body is consumed from ordinary tokens and joined with
spaces deterministically. That string does not claim to preserve formatting or per-instruction
lines and columns: the lexical mode and exact locations explicitly belong to item 288.

Los braces internos se balancean para que la frontera externa no se pierda y para dejar espacio a
formas futuras sin comprometer todavía una sintaxis de operandos. Este commit no crea HIR, MIR ni
código LLVM para asm; hacerlo aquí mezclaría los ítems 289–300 y ocultaría qué capa introdujo cada
garantía. / Inner braces are balanced so the outer boundary is not lost and future forms retain
design room without committing to operand syntax yet. This commit creates no HIR, MIR, or LLVM code
for assembly; doing so here would conflate items 289–300 and obscure which layer introduced each
guarantee.

## Evidencia enfocada / Focused evidence

```text
ctest -R ^nsc.integrated-assembly.grammar$
12 checks passed
```

No se ejecutó baseline en este commit; corresponde al cierre de la tanda 4. / No baseline was run
for this commit; it belongs to the end-of-batch 4 closure.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Introduce la gramática estructural de asm / Introduce structural asm grammar`

**Descripción / Description**

`Reconoce asm como sentencia contextual con bloque balanceado, nodo AST propio, recuperación precisa y pruebas sintácticas, sin adelantar todavía lexer ni lowering. / Recognize asm as a contextual balanced-block statement with its own AST node, precise recovery, and syntax tests, without prematurely adding lexer or lowering semantics.`
