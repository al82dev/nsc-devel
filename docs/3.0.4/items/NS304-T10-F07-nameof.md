# NS304-T10-F07 — `nameof` para símbolos, miembros, aliases y genéricos

- **Estado**: CERRADO
- **Rama**: development

## Resultado

- **Parser/AST** (`parseNameOfExpression`, `src/Parser/Parser.cpp`): `nameof(x)` se reconoce como
  keyword contextual y conserva el operando en `NameOfExpression`; los argumentos genéricos se
  retienen hasta distinguir un tipo de un método. Cubre:
  - `nameof(ident)` → `"ident"` (símbolos y aliases `using`, que también son identificadores);
  - `nameof(a.b)` / `nameof(a.b.c)` → nombre del último miembro;
  - `nameof(m(...))` / `nameof(x[...])` → nombre del callee/receptor;
  - `nameof(Generic<int>)` y métodos genéricos cualificados/no cualificados.
- **Resolución**: `ProjectGenericModel` materializa sólo nombres que corresponden a templates de
  tipo; `ProjectNameLookup` y `ProjectMethodBodyModel` resuelven tipos, miembros y templates de
  método, validan aridad/accesibilidad y restauran el flujo de ownership del operando no evaluado.
- **Lowering**: HIR produce la constante de cadena únicamente después de una resolución válida;
  `nameof(SideEffect())` no emite ni ejecuta la llamada.
- **Evidencia focal (2026-10-13)**: `TA-AST-025`, `TL-LANG-331`, `TL-LANG-350`, `TL-LANG-353` y
  `TL-LANG-356` PASS. Los casos cubren spellings simples/cualificados, tipos y métodos genéricos,
  ausencia de efectos laterales y rechazo de símbolo inexistente, miembro inaccesible y aridad
  genérica inválida.
