# NS304-T09-F01 — Miembros `required` y definite initialization

- **Estado**: PARCIAL (parser + flag `isRequired`) / residual (validación en construcción)
- **Rama**: development

## Resultado

- **Parser** (`src/Parser/Parser.cpp`): `required` se reconoce como modificador contextual en la
  lista de modificadores de miembro de clase. El campo se marca con el sufijo `required` en su
  `auxiliaryText` (junto a `at:`/`field-align:`).
- **Member model** (`ProjectMemberModel.h/.cpp`): nuevo flag `ClassMemberSymbol::isRequired`
  derivado del marcador `required`.
- Prueba: `TL-LANG-335` (`public required int Value;` compila y se comporta como campo ordinario).

## Residual

- **Validación de definite initialization**: exigir que todo miembro `required` se inicialice en el
  constructor o object initializer; hoy `required` es solo un marcador sin enforcement.
