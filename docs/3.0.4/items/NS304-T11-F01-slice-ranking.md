# NS304-T11-F01 — Ranking de slice/span

- **Estado**: CERRADO (vista `Span<T>`→`ReadOnlySpan<T>` unidireccional; identidad y mutabilidad
  preservadas; la inversa se rechaza)
- **Rama**: development

## Resultado (2026-10-14)

- `ProjectConversionKind::View` nuevo. `ProjectConversionClassifier::classifyImplicit` reconoce la
  terminal `Span__g1__`→`ReadOnlySpan__g1__` (misma aridad y mismo elemento) y devuelve una
  conversión `View` (rank 6, por detrás de referencia, por delante de user-defined). La inversa
  (`ReadOnlySpan`→`Span`) no tiene regla y se rechaza, preservando la mutabilidad.
- Lowering estructural en `ProjectOopLlvmLowerer::convert` (copia los dos campos, el puntero por
  identidad en punteros opacos y la longitud por identidad). Sin copia de heap.
- Prueba puntual **TL-LANG-408** (exit 7: `Span<int>`→`ReadOnlySpan<int>` preserva 3+4) + guards
  TL-LANG-359/360 (list pattern span) PASS.

## Auditoría (línea base)

Contrato (tracking): *Implementar ranking de slice/span*.
Plan T11-F01 (DeepSeekDocs): *reconocer mutable e inmutable como conversiones propias; preferir
identidad, preservar mutabilidad y evitar que una conversión de vista gane frente a una mejor
conversión nominal; documentar ranking y ambigüedades*.

Estado real:

- `lib/NativeSharp.Core/Span.ns`, `StringView` y los modelos genéricos/ownership existen como
  insumos, no como integración completa.
- `ProjectConversionClassifier.*` rankea conversiones nominales; la vista mutable/inmutable ahora
  tiene su regla propia (rank 6) y la inversa se rechaza.

## Gap

CERRADO. Falta aún documentar el ranking completo de vistas (antes de F02 conversiones sin copia)
si nuevas vistas (StringView, etc.) entran; hoy solo `Span`→`ReadOnlySpan` está normado.

## Nota

F01 es la base del ranking de vistas (antes de F02 conversiones sin copia). No es una surface nueva;
es el classifier + el contrato de mutabilidad/provenance.
