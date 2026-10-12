# NS304-T03-F04 — Property patterns anidados, posicionales, relacionales y lógicos

- **Estado**: EN IMPLEMENTACIÓN
- **Rama**: development
- **Alcance (SCOPE LOCK)**: solo `Parser::parsePattern*`, `Parser::makePatternCondition`,
  `Parser::emitPatternBindings`, `MethodCoreOperationKind` si se necesita un op nuevo, y la
  prueba `TL-LANG-311`. NO tocar CMake, toolchain, Builder, foreach, rango T01-F04, ni los
  commits de longitud/vista de F03.

## Auditoría (línea base)

Grammar actual de `parsePatternPrimary` (Parser.cpp:10466):

- `_` → Wildcard
- `[ ... ]` → List
- `var x` / `move x` / `ref x` → Binding
- literal → Literal; `lit .. lit` / `lit ..= lit` → Range
- `( ... )` → Tuple / agrupado
- `{ campo: subpatrón, ... }` → Struct (property)
- `Nombre { ... }` → Struct con nombre
- `Nombre ( payload )` → Variant (un solo payload; >1 se rechaza)
- `a | b` → Or

## Gaps F04

1. **Relacionales** (`< 5`, `<= 5`, `> 5`, `>= 5`): falta parseo (solo `..`/`..=` de rango) y
   falta lowering en `makePatternCondition`.
2. **Lógicos `and`/`not`**: falta `and` (conjunción) y `not` (negación); solo existe `or`.
3. **Posicionales** `T(a, b, ...)`: el `Ident(...)` actual es `Variant` de un payload; falta el
   modo posicional multi-argumento.
4. **Anidados**: la recursión de parseo ya existe; hay que confirmar que el lowering
   (`makePatternCondition`/`emitPatternBindings`) baja sub-patrones Struct/Relacional correctamente.

## Plan (orden de implementación)

1. Relacionales: nuevo `PatternKind::Relational` + parseo de `< <= > >=` + lowering (compara
   el scrutinee contra el operando con el operador). Es el más autocontenido.
2. Lógicos: `not` (unario) y `and` (binario) como `PatternKind::Not`/`PatternKind::And`,
   con lowering booleano.
3. Posicionales: reconocer `T(a, b, ...)` multi-argumento y bajar a un índice/constructor por
   posición (requiere decidir el contrato: constructor de tipo o tupla).
4. Anidados: prueba de regresión del lowering de Struct anidado.

## Prueba puntual

`TL-LANG-311` (a crear en LanguageSurfaceModule.cpp + cases.jsonl + CSVs): property anidado,
relacional, lógico (and/not) y posicional sobre un struct de prueba.
