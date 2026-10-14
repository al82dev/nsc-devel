# NS304-T10-F01 — Literales UTF-8 tipados y forma allocation-free

- **Estado**: IMPLEMENTADO (`u8"..."` → `ReadOnlySpan<byte>`, bytes UTF-8 en constante estática)
- **Rama**: development

## Surface

- `u8"..."` es un literal **tipado**: produce `ReadOnlySpan<byte>` sobre los bytes UTF-8 del literal
  (los escapes `\u`/`\U`/`\e`/`\x` decodifican a UTF-8). No es `string` ni asigna heap en runtime: el
  lowering interna los bytes en un global estático y construye el struct `{ byte* start, int count }`.
- `u8` sigue siendo el keyword de `byte`; `u8"` (seguido de comilla) es el prefijo del literal.

## Implementación

- Lexer: `TokenKind::Utf8StringLiteral` + `Lexer::lexUtf8String` (`u8"` reconocido antes del
  identifier/keyword).
- Parser: `StringLiteralExpression` con `op=Utf8StringLiteral`.
- Sema: `Utf8StringLiteral` → `MethodCoreOperationKind::Utf8StringLiteral`, tipo cerrado
  `ReadOnlySpan<builtin:byte>` vía `closedTypeKeyForTemplate`.
- HIR/MIR/LLVM: nodo `Utf8StringLiteral` → `ReadOnlySpan<byte> { start = &literal.bytes, count = N }`
  (reutiliza `internStringLiteral`, sin heap).

## Prueba

- `TL-LANG-473` — `u8"hello".Length()` (5) + `u8"\u00E9".Length()` (2) → exit 52.

## Commits

- `82d85d29` — literal `u8"..."` como `ReadOnlySpan<byte>`.
