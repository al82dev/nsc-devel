# NS304-T10-F07 — `nameof` para símbolos, miembros, aliases y genéricos

- **Estado**: CERRADO
- **Rama**: development

## Resultado

- **Parser** (`parseNameOfExpression`, `src/Parser/Parser.cpp`): `nameof(x)` se reconoce como
  keyword contextual en `parsePrimaryExpression` y se baja a un `StringLiteralExpression` con el
  nombre del último identificador/miembro. Cubre:
  - `nameof(ident)` → `"ident"` (símbolos y aliases `using`, que también son identificadores);
  - `nameof(a.b)` / `nameof(a.b.c)` → nombre del último miembro;
  - `nameof(m(...))` / `nameof(x[...])` → nombre del callee/receptor;
  - `nameof(Generic<int>)` → `"Generic"` (tipo genérico cerrado, resuelto vía `parseType` +
    `baseName()` antes de la caída a expresión).
- Reutiliza la maquinaria existente de literales de cadena (Sema `string-literal` → LLVM), sin
  tocar Sema ni lowering.
- Prueba: `TL-LANG-331` (`nameof(Program)` = "Program" + `nameof(Program.Main)` = "Main" +
  `nameof(Box<int>)` = "Box", 7+4+3=14).
