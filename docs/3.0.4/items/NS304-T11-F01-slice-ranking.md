# NS304-T11-F01 — Ranking de slice/span

- **Estado**: AUDITADO (Span/StringView son insumos; ranking mutable/inmutable sin normativa)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar ranking de slice/span*.
Plan T11-F01 (DeepSeekDocs): *reconocer mutable e inmutable como conversiones propias; preferir
identidad, preservar mutabilidad y evitar que una conversión de vista gane frente a una mejor
conversión nominal; documentar ranking y ambigüedades*.

Estado real:

- `lib/NativeSharp.Core/Span.ns`, `StringView` y los modelos genéricos/ownership existen como
  insumos, no como integración completa.
- `ProjectConversionClassifier.*` rankea conversiones nominales; no hay una regla propia para las
  vistas mutable/inmutable (slice/span) que preserve la mutabilidad y la preferencia de identidad.

## Gap

Falta el ranking normativo de slice/span: mutable/immutable como conversiones propias, preferir
identidad, preservar mutabilidad (inmutable acepta fuente mutable; mutable exige storage mutable), y
no dejar que una vista gane frente a una conversión nominal mejor.

## Nota

F01 es la base del ranking de vistas (antes de F02 conversiones sin copia). No es una surface nueva;
es el classifier + el contrato de mutabilidad/provenance.
