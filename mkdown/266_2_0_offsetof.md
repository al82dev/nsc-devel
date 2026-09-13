# Entrega 266 — `offsetof` / Delivery 266 — `offsetof`

## Resumen / Summary

Se incorporó la expresión constante `offsetof(T, campo)`. Devuelve `nuint` y obtiene el offset
físico desde el modelo canónico de layout. No crea objetos, no evalúa una expresión receptora y no
genera cálculo en tiempo de ejecución.

The compiler now supports the constant expression `offsetof(T, field)`. It returns `nuint` and
reads the physical byte offset from the canonical object-layout model, with no runtime code.

## Archivos creados / Created files

- `tests/unit/OffsetofTests.cpp`: casos OOP para layout con padding, especialización genérica,
  campo inexistente y rechazo de clases.

## Archivos modificados / Modified files

- Lexer, AST y parser: palabra reservada `offsetof` y nodo `OffsetofExpression` con `TypeSyntax` y
  nombre de campo.
- `SemanticAnalyzer`: resolución clásica de struct/campo, diagnóstico `InvalidOffsetof` y folding
  inmediato a una constante `nuint`.
- `ProjectMethodBodyModel`: resolución nominal del tipo cerrado y selección del ordinal físico del
  campo, conservados en `MethodCoreBinding`.
- `ProjectOopHirModel`: consulta tardía de `fieldLayoutForMember()` y materialización de un
  `IntegerConstant`; MIR y LLVM reciben una constante ordinaria.
- Pruebas unitarias existentes, CMake, gramática, especificación, libro, changelog, mapa de
  conformidad y roadmap 2.0.

## Implementación / Implementation

La sintaxis elegida es `offsetof(T, campo)`, equivalente a la intención de C pero coherente con el
sistema de tipos NativeSharp. El parser exige un tipo y un identificador; no acepta una expresión
receptora. El frontend clásico ya conoce offsets al terminar el layout de structs y los pliega de
inmediato. En el pipeline OOP, el análisis de cuerpos ocurre antes del layout: por eso almacena el
tipo cerrado y el ordinal del campo, y HIR consulta el offset cuando `ProjectObjectLayoutModel` ya
está disponible.

The classic frontend folds the offset immediately because its struct symbols already carry field
offsets. The OOP pipeline intentionally delays only the numeric lookup: semantic analysis records
the closed type and field ordinal, then HIR asks the canonical layout model for the byte offset.

## Decisiones de diseño / Design decisions

- Sólo structs/unions: una clase incluye cabecera e infraestructura OOP y no representa un value
  layout interoperable mediante `offsetof`.
- Sólo campos de instancia directos: los estáticos no ocupan bytes dentro del valor y un nombre
  inexistente produce `InvalidOffsetof`.
- Genéricos cerrados: el ordinal se resuelve después de la monomorfización, de modo que padding y
  alineación corresponden a la especialización concreta.
- Una autoridad de layout: no se duplican reglas de padding, `pack`, alineación u offsets
  explícitos en el parser ni en semántica.

## Evidencia dirigida / Focused evidence

- `nsc_lexer_tests`: 39 checks.
- `nsc_parser_tests`: 21 checks.
- `nsc_semantic_tests`: 126 checks.
- `nsc_offsetof_tests`: 4 checks.
- No se ejecutó baseline; corresponde al cierre de la tanda 2.

## Commit de referencia / Reference commit

**Título / Title**

`Añade offsetof sobre el layout canónico / Add offsetof over canonical layout`

**Descripción / Description**

`Incorpora offsetof(T, campo) como constante nuint, resuelve campos de structs y especializaciones cerradas y obtiene su desplazamiento del modelo canónico de layout. / Adds offsetof(T, field) as a nuint constant, resolves fields on structs and closed specializations, and reads their displacement from the canonical layout model.`
