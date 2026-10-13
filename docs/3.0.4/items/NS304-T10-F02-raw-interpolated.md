# NS304-T10-F02 — Raw interpolated strings y delimitadores anidados

- **Estado**: CERRADO (lexer + integración) / residual (interpolación con huecos runtime)
- **Rama**: development

## Resultado

- **Lexer** (`lexRawInterpolatedString`, `src/Lexer/Lexer.cpp`): `$"""..."""` (y cualquier
  ancho de delimitador `3+` comillas) se reconoce antes de `lexInterpolatedString` y se lexea como
  un `InterpolatedStringLiteral` con contenido **crudo** (sin decodificar escapes) y huecos
  `{expr}` / llaves literales `{{`/`}}` preservados.
- **Parser/Sema**: el token resultante reutiliza la maquinaria existente de interpolación estática
  (T01-F06); un `$"""..."""` sin huecos compila y ejecuta de punta a punta.
- Pruebas: `TL-LEX-111` (lexer, contenido crudo `a\n{hole}b`) y `TL-LANG-332` (integración,
  `$"""answer"""` = "answer" → 6).

## Residual

- Huecos **runtime** (no consteval) en raw interpolated strings: el handler estático T01-F06 los
  rechaza, igual que en los interpolated strings ordinarios. Queda fuera de este ítem (T10-F03).
