# Tanda 01 — Expresiones y ergonomía / Expressions and Ergonomics

> **Actualización 2026-09-29:** `IMPLEMENTING`. F01, F02 y el slice trivial seguro de F05 ya
> atraviesan binding, HIR, MIR, LLVM y NSMETA 1.24 mediante la tanda operativa 05. La tanda
> operativa 06 conecta el vertical ABI-seguro de F03/F04/F06/F07: null-aware de carriers pointer,
> índice `^`, interpolación totalmente constante y `with` trivial. Optional/value `?.`, slicing
> por views, holes runtime, owning `with`, tooling y cleanup no trivial permanecen asignados a sus
> dependencias posteriores. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

Esta tanda establece la superficie sintáctica y el contrato normativo compartido para las siete
familias de expresiones de NativeSharp 3.0. Las formas nuevas quedan separadas en el AST, están
protegidas por la edición del proyecto y conservan orden de evaluación, evaluación única y ausencia
de asignaciones implícitas como invariantes consultables por las fases posteriores.

This batch establishes the syntax surface and shared normative contract for all seven NativeSharp
3.0 expression families. New forms remain distinct in the AST, are gated by the project language
edition, and expose source order, single evaluation, and no implicit allocation as queryable
invariants for downstream phases.

## Cambios / Changes

- Los argumentos nombrados existentes se conservaron como la ruta canónica; `AstDumper` ahora
  publica el nombre para que snapshots y diagnósticos puedan comprobar el mapeo.
- `params` y `with` son palabras contextuales. Código 2.x puede seguir usándolas como nombres.
- `params ReadOnlySpan<T>` se conserva en la firma y se valida en métodos, constructores y
  funciones libres: debe ser el último parámetro, no admite `ref`/`out`/`in`, no admite valor por
  defecto y nunca se convierte silenciosamente en un arreglo de heap.
- El lexer reconoce `??`, `??=`, `?.`, `..` y `$"..."`. Los tokens se añadieron al final del enum
  para no cambiar identidades 2.x; `...` continúa siendo el token variádico C independiente.
- El parser implementa precedencia asociativa derecha para coalescencia, rangos abiertos,
  endpoints `^n`, acceso condicional, miembros/spreads de inicializador, interpolación estática y
  actualización `with`.
- Cada construcción conserva un nodo AST propio cuando desazucarar temprano podría duplicar
  efectos: `NullCoalescingExpression`, `NullConditionalMemberExpression`, `RangeExpression`,
  `FromEndExpression`, `SpreadElementExpression`, `InterpolatedStringExpression` y
  `WithExpression`.
- La edición 2 rechaza todas las formas nuevas con el diagnóstico estable `NS3002`; la edición 3
  las acepta. Proyectos sin `<Language>` continúan en edición 2.
- `ExpressionErgonomics` centraliza disponibilidad por edición, contratos de lowering, detección
  de `ReadOnlySpan<T>`, clasificación null-aware, resolución half-open de rangos y separación de
  literales/holes/formatos constantes de interpolación.

- Existing named arguments remain the canonical path; `AstDumper` now publishes argument names so
  snapshots and diagnostics can verify mapping.
- `params` and `with` are contextual keywords, preserving valid 2.x identifiers.
- `params ReadOnlySpan<T>` is retained and validated in method, constructor, and free-function
  signatures: it must be last, cannot combine with `ref`/`out`/`in`, cannot have a default, and
  never implies a heap array.
- The lexer recognizes `??`, `??=`, `?.`, `..`, and `$"..."` while preserving C `...`.
- The parser records right-associative coalescing, open ranges, from-end endpoints, conditional
  access, generalized initializer entries, static interpolation, and `with` updates.
- Edition 2 reports stable `NS3002`; edition 3 accepts the new surface.

## Archivos nuevos / New files

- `include/nativesharp/Language/ExpressionErgonomics.h`: contrato público y tipos de planificación.
- `src/Language/ExpressionErgonomics.cpp`: reglas compartidas y parser de interpolación estática.
- `tests/unit/NativeSharp3T01ExpressionTests.cpp`: prueba dirigida de las siete familias.

## Archivos modificados / Modified files

- `CMakeLists.txt`
- `include/nativesharp/AST/AstNodeKind.h`
- `include/nativesharp/Frontend/ProjectMemberModel.h`
- `include/nativesharp/Lexer/Lexer.h`
- `include/nativesharp/Lexer/TokenKind.h`
- `include/nativesharp/Parser/Parser.h`
- `src/AST/AstDumper.cpp`
- `src/AST/AstNodeKind.cpp`
- `src/Frontend/FrontendCompilation.cpp`
- `src/Frontend/ProjectMemberModel.cpp`
- `src/Frontend/ProjectNamespaceFunctionModel.cpp`
- `src/Frontend/ProjectSyntaxUnit.cpp`
- `src/Lexer/Lexer.cpp`
- `src/Lexer/TokenKind.cpp`
- `src/Parser/Parser.cpp`

## Decisiones / Decisions

1. No se desazucaran expresiones con efectos en el parser. El nodo explícito evita duplicar el
   receptor de `?.`, el lado izquierdo de `??` o la fuente de `with`.
2. `params` sólo describe un `ReadOnlySpan<T>` físico. La ABI sigue recibiendo un span y el
   contrato prohíbe fabricar un arreglo administrado implícito.
3. Los rangos son half-open. `^0` resuelve a `Length`; un endpoint fuera de `[0, Length]` o un
   inicio mayor al fin es inválido.
4. La interpolación conserva el template completo y separa holes en compile time. `{{` y `}}`
   siguen siendo literales y un hole vacío o desbalanceado es diagnóstico.
5. La integración ejecutable de estos nodos con el modelo OOP, MIR y LLVM se valida en la tanda de
   integración del lenguaje; hasta entonces los nodos permanecen explícitos para que ninguna fase
   los confunda con una operación 2.x y genere código incorrecto.

## Mejora obtenida / Improvement

- Una sola autoridad define costos y orden de evaluación para toda la familia T01.
- El lexer y parser ya no necesitan reinterpretar secuencias ambiguas en capas tardías.
- La compatibilidad de fuentes 2.x queda protegida tanto por edición como por keywords contextuales.
- Los dumps contienen suficiente estructura para snapshots futuros de HIR/MIR.
- Las reglas sin heap pueden comprobarse sin ejecutar el backend.

## Validación dirigida / Targeted validation

- `nsc` construido correctamente.
- `nsc_ns3_t01_expression_tests`: **27/27** checks.
- `NSC_BUILD_TESTS` restaurado a `OFF`.
- No se ejecutó CTest, baseline ni una suite general.
