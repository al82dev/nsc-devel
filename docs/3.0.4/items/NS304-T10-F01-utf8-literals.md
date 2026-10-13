# NS304-T10-F01 — Literales UTF-8 tipados y forma allocation-free

- **Estado**: PARCIAL (escapes UTF-8 vía T10-F08) / residual (tipo UTF-8 tipado `u8"..."`
  / `Utf8`/`ReadOnlySpan<byte>`)
- **Rama**: development

## Resultado

- La codificación UTF-8 de los literales de cadena ya está cubierta por **T10-F08** (`\e`, `\u`/
  `\U`, prueba noheap `TL-LANG-333`): un literal con escapes produce la secuencia UTF-8 correcta sin
  asignación en runtime (constante de tiempo de compilación).

## Residual

- **Tipo UTF-8 tipado**: `u8"..."` (literal UTF-8 con tipo distinto de `string`, p. ej.
  `ReadOnlySpan<byte>`/`Utf8`) no existe; `u8` es hoy el keyword del tipo `byte`. Requiere lexer
  (`u8"` como prefijo de literal) + un tipo UTF-8 dedicado y su conversión sin copia.
