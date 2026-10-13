# NS304-T10-F07 — `nameof` para símbolos, miembros, aliases y genéricos no ligados

- **Estado**: PARCIAL (símbolos + miembros) / residual (aliases y genéricos no ligados)
- **Rama**: development

## Resultado

- **Parser** (`parseNameOfExpression`, `src/Parser/Parser.cpp`): `nameof(x)` se reconoce como
  keyword contextual en `parsePrimaryExpression` y se baja a un `StringLiteralExpression` con el
  nombre del último identificador/miembro. Cubre:
  - `nameof(ident)` → `"ident"`;
  - `nameof(a.b)` / `nameof(a.b.c)` → nombre del último miembro;
  - `nameof(m(...))` / `nameof(x[...])` → nombre del callee/receptor.
- Reutiliza la maquinaria existente de literales de cadena (Sema `string-literal` → LLVM), sin
  tocar Sema ni lowering.
- Prueba: `TL-LANG-331` (`nameof(Program)` = "Program" + `nameof(Program.Main)` = "Main", 7+4=11).

## Residual

- `nameof` sobre **tipos genéricos no ligados** (`nameof(List<int>)` → "List") y **aliases**
  (`using`) requieren resolver el argumento como tipo, no como expresión.
