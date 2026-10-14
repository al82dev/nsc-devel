# NS304-T10-F08 — `\e`, escapes Unicode/UTF-8 y pruebas `noheap`/`no-culture`

- **Estado**: IMPLEMENTADO (`\e` + `\u`/`\U` + prueba `noheap`; `no-culture` bloqueado en feature ajena)
- **Rama**: development

## Resultado

- **`\e` (ESC, 0x1B/27)**: agregado a `decodeEscapeByte` (`src/Lexer/Lexer.cpp`) como escape
  simple de un byte, siguiendo C# 14. Cubre char literal y string literal (ambas rutas pasan por
  `decodeEscapeByte`).
- **`\uXXXX` / `\UXXXXXXXX`**: `decodeUnicodeCodePoint` valida scalar value (sin sustitutos, hasta
  U+10FFFF) y codifica UTF-8 en strings; en char literal exige un byte ASCII (<= 0x7F).
- Pruebas: `TL-LEX-057` (char literals, `\e` + `\u0041`), `TL-LEX-066` (string literals, `\e` +
  `\u00E9` + `\U0001F600`) y `TL-LANG-333` (`noheap` string literal con `\u`/`\e`) → **PASS**.

## Residual (bloqueado, fuera del scope de este ítem)

- Prueba `no-culture`: depende de la semántica culture-invariant (atributo/contexto `no-culture`),
  que no existe aún en 3.0.4. Es una feature aparte; no bloquea el escape UTF-8 de este ítem.
