# NS304-T15-F08 — Formatting, traits, generics y bordes decimal

- **Estado**: PARCIAL (core-op `DecimalToString` plumbing armado; emisión de dígitos del lowering pendiente)
- **Rama**: development

## Resultado parcial (2026-10-14)

- **Core-op**: `String.ToString(decimal)` se reconoce como operación núcleo (`DecimalToString`) con su
  nodo HIR/MIR y la firma en `String.ns`; el round-trip baja al LLVM.
- **Bloqueo**: el emisor de dígitos (coeficiente+escala → "±<int>.<frac>") revienta el lowering; no se
  commiteó (surface a medio construir revertida).

## Residual

- Terminar la emisión de dígitos (reusar `String.ToString(long)` compilado o emitir dígitos sin
  defecto) + el parsing/formatting culture-free + traits numéricos/generics sin boxing + el corpus de
  bordes.
