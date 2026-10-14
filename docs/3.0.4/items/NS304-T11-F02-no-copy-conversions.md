# NS304-T11-F02 — Conversiones sin copia

- **Estado**: PARCIAL (array→`Span<T>`/`ReadOnlySpan<T>` sin copia ✅; buffer/string→slice pendiente)
- **Rama**: development

## Resultado (slice array→span — 2026-10-14)

- `ProjectConversionClassifier::classifyImplicit`: `builtin:<T>[]` → `Span<T>`/`ReadOnlySpan<T>` como
  conversión `View` (rank 7), con el elemento emparejado por spelling sanitizado (`builtin:int`→`int`).
- Lowering reutiliza `SpanFromArray`/`ReadOnlySpanFromArray` existente: `wrapImplicitConversion`
  sintetiza `[array, 0]` (2 hijos = array completo) y el LLVM deriva `count` de la cabecera de
  longitud del propio objeto. Sin arreglo temporal ni copia de heap.
- Prueba puntual **TL-LANG-409** (exit 14: `int[]{3,4}` → `Span<int>`+`ReadOnlySpan<int>` preserva
  3+4+3+4) + guards TL-LANG-359/360/408 PASS. La inversa (span→array) sigue sin regla.

## Residual

- **buffer/string → slice**: `StringView` (y buffer) → `Span`/`ReadOnlySpan` sin copia todavía no
  integrados (requieren la operación núcleo de cadena, análoga a `SpanFromArray`).
- **Lifetime (destino ≤ fuente)** y provenance completa.

## Auditoría (línea base)

Contrato (tracking): *Implementar conversiones sin copia*.
Plan T11-F02 (DeepSeekDocs): *array, buffer, string y slice producen base/offset/length/provenance;
nunca array temporal; inmutable puede aceptar fuente mutable sin conceder escritura; mutable exige
storage mutable; lifetime destino no excede fuente*.

Estado real:

- `Span.ns`/`StringView` son vistas insumo; la conversión array → slice ya está integrada al
  classifier y al lowering `SpanFromArray` (sin copia). StringView/buffer pendientes.

## Gap

Falta la conversión sin copia de buffer/string → slice y las reglas de lifetime (destino ≤ fuente).
El slice array→span quedó cerrado.

## Nota

F02 depende de F01 (ranking) y es el lowering de la vista. No es una surface nueva; es la conversión
no-copy + el contrato mutability/lifetime.
