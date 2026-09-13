# Entrega 265 — `static_assert` / Delivery 265 — `static_assert`

## Resumen / Summary

Se incorporó una aserción de compilación sin representación de ejecución:

```csharp
static_assert(sizeof(Header) == 16u, "Header ABI changed");
```

The compiler now accepts a compile-time assertion with no runtime representation. A false,
non-boolean, or runtime-dependent condition rejects the program before object generation.

## Archivos creados / Created files

- `tests/unit/StaticAssertTests.cpp`: prueba dirigida del pipeline de proyecto para aserciones
  verdaderas, falsas, dependientes de runtime y con tipo incorrecto.

## Archivos modificados / Modified files

- `include/nativesharp/Lexer/TokenKind.h`, `src/Lexer/Lexer.cpp` y
  `src/Lexer/TokenKind.cpp`: palabra reservada `static_assert`.
- `include/nativesharp/AST/AstNodeKind.h` y `src/AST/AstNodeKind.cpp`: nodo
  `StaticAssertStatement`.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`: gramática
  `static_assert(expression[, string-literal]);`.
- `include/nativesharp/Diagnostics/DiagnosticId.h`,
  `include/nativesharp/Sema/SemanticAnalyzer.h` y `src/Sema/SemanticAnalyzer.cpp`: tipo `bool`,
  constancia y diagnóstico `StaticAssertionFailed` en el frontend clásico.
- `src/Frontend/ProjectMethodBodyModel.cpp`: validación de tipo y binding de la condición en el
  pipeline OOP.
- `src/Frontend/ProjectOopHirModel.cpp`: evaluación tardía contra el layout canónico y eliminación
  de la aserción antes de MIR.
- `src/HIR/HirBuilder.cpp`: eliminación equivalente en el pipeline clásico.
- `CMakeLists.txt`: registro de `nsc.language.static-assert`.
- Documentación de lenguaje, gramática, libro, changelog, mapa de conformidad y roadmap 2.0.

## Decisiones de diseño / Design decisions

### Evaluación tardía del layout / Late layout evaluation

El modelo de cuerpos se construye antes del modelo de layout. Calcular ahí `sizeof` o `alignof`
obligaría a duplicar tamaños, alineaciones y padding. La condición se tipa durante semántica y se
evalúa al construir OOP HIR, donde ya existe `ProjectObjectLayoutModel`. De esa forma la aserción,
el stride de arreglos, los offsets y el backend consultan una sola autoridad.

Method-body semantics runs before object layout. Evaluating layout queries there would introduce a
second ABI table. The condition is type-checked early and evaluated while building OOP HIR, where
the canonical object-layout model is available.

### Cero costo de ejecución / Zero runtime cost

Una aserción correcta se sustituye por un bloque HIR vacío. La expresión no llega a MIR, LLVM ni al
objeto, por lo que no puede conservar llamadas, cargas o efectos laterales escondidos. Una
expresión que no puede reducirse completamente a constante se rechaza.

A successful assertion becomes an empty HIR block. Its expression never reaches MIR, LLVM, or the
object file. Runtime-dependent expressions are rejected instead of being emitted as checks.

### Alcance sintáctico / Syntactic scope

La entrega define `static_assert` como statement dentro de bloques. No introduce una declaración
global ni un miembro de tipo. Esto mantiene la aserción dentro del pipeline que ya resuelve nombres,
genéricos especializados y layouts del método consumidor.

This delivery deliberately defines a block statement, not a namespace or type declaration.

### Contrato numérico existente / Existing numeric contract

`sizeof` y `alignof` devuelven `nuint`. No se añadieron conversiones especiales sólo para hacer más
corta una aserción; los ejemplos usan `4u`, `8u`, etc. La evaluación tardía conserva wrap, signo,
shifts y el caso definido `MinValue / -1` sin provocar comportamiento indefinido dentro de NSC.

## Evidencia dirigida / Focused evidence

- `nsc_lexer_tests`: 39 checks.
- `nsc_parser_tests`: 21 checks.
- `nsc_semantic_tests`: 123 checks.
- `nsc_static_assert_tests`: 4 checks.
- No se ejecutó baseline; corresponde únicamente al cierre de la tanda 2.

## Commit de referencia / Reference commit

**Título / Title**

`Añade aserciones estáticas de compilación / Add compile-time static assertions`

**Descripción / Description**

`Incorpora static_assert como statement sin costo de ejecución, evalúa sizeof/alignof contra el layout canónico y diagnostica condiciones falsas, no constantes o de tipo incorrecto. / Adds static_assert as a zero-runtime-cost statement, evaluates sizeof/alignof against canonical layout, and diagnoses false, non-constant, or incorrectly typed conditions.`
