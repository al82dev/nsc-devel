# NS304-T10-F08 — `\e`, diagnósticos Unicode/UTF-8 y pruebas `noheap`/`no-culture`

- **Estado**: PARCIAL (`\e` implementado) / residual (diagnósticos Unicode/UTF-8 y pruebas
  `noheap`/`no-culture`)
- **Rama**: development

## Resultado

- **`\e` (ESC, 0x1B/27)**: agregado a `decodeEscapeByte` (`src/Lexer/Lexer.cpp`) como escape
  simple de un byte, siguiendo C# 14. Cubre char literal y string literal (ambas rutas pasan por
  `decodeEscapeByte`).
- Pruebas: `TL-LEX-057` (char literals) y `TL-LEX-066` (string literals) extendidas con `\e` (27).

## Residual

- Diagnósticos de secuencias Unicode/UTF-8 (fuera del alcance de `decodeEscapeByte`).
- Pruebas `noheap`/`no-culture` para literales y escapes.
