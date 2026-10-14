# NS304-T15-F01 — Representación decimal

- **Estado**: AUDITADO (NumericSemantics/lexer/parser son infraestructura; representación decimal sin definir)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar la representación decimal*.
Plan T15-F01 (DeepSeekDocs): *documentar bits, invariantes, canonicalization, precision/scale,
equality, hashing, rounding y excepciones numéricas; asserts/layout tests y ABI classification; no
permitir bit patterns reservados desde safe code*.

Estado real:

- `Language/NumericSemantics`, lexer/parser y `String.ns` son infraestructura, no implementación
  decimal. No hay `decimal` nativo (128 bits: coeficiente 96 bits + escala 0–28 + signo + bits
  reservados).

## Gap

Falta congelar el contrato numérico de `decimal` (bits/invariantes/canonicalización/precision/scale/
equality/hashing/rounding/excepciones) + los asserts/layout tests + la ABI classification (sin
bit patterns reservados desde safe code).

## Nota

F01 es la base de T15 (antes de F02 literals + F03 ABI + F04 aritmética). El contrato precede código
y helpers. No es una surface nueva; es la definición del tipo `decimal`.
