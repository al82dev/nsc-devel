# NS304-T15-F04 — Aritmética y comparación decimal

- **Estado**: AUDITADO (aritmética de enteros/FP existe; aritmética decimal sin implementar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar aritmética y comparación decimal*.
Plan T15-F04 (DeepSeekDocs): *add/sub/mul/div/rem/compare con enteros anchos o helpers exactos,
alineación de escalas, rounding y overflow definidos; no usar `double` como intermedio; probar
identidades, signos, escalas extremas y división periódica*.

Estado real:

- La aritmética de enteros/FP existe; no hay operaciones `decimal` (coeficiente 96 bits + escala) con
  alineación de escalas + rounding/overflow.

## Gap

Falta add/sub/mul/div/rem/compare decimales (enteros anchos exactos, sin `double`), alineación de
escalas, rounding y overflow definidos, y el corpus de identidades/signos/escalas/división periódica.

## Nota

F04 depende de F01 (representación) + F03 (ABI). No es una surface nueva; es la aritmética exacta del
coeficiente 96 bits.
