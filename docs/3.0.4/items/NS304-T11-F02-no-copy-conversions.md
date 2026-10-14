# NS304-T11-F02 — Conversiones sin copia

- **Estado**: PARCIAL (array→`Span`/`ReadOnlySpan` ✅; string→`StringView` ✅; buffer→slice y lifetime pendiente)
- **Rama**: development

## Resultado (2026-10-14)

- **array→span**: `builtin:<T>[]` → `Span<T>`/`ReadOnlySpan<T>` (`View` rank 7), lowering reutiliza
  `SpanFromArray` con 2 hijos (array completo, count derivado de la cabecera). TL-LANG-409 exit 14.
- **string→StringView**: `builtin:string` → `StringView` (`View` rank 7), lowering reutiliza
  `StringViewFromString` con 2 hijos (cadena completa). TL-LANG-410 exit 5.
- Sin arreglo temporal ni copia de heap. La inversa (span→array, StringView→string) sigue sin regla.

## Residual

- **buffer → slice** (bloque/stackalloc crudo) y **lifetime (destino ≤ fuente)** + provenance completa.

## Auditoría (línea base)

Contrato (tracking): *Implementar conversiones sin copia*.
Plan T11-F02 (DeepSeekDocs): *array, buffer, string y slice producen base/offset/length/provenance;
nunca array temporal; inmutable puede aceptar fuente mutable sin conceder escritura; mutable exige
storage mutable; lifetime destino no excede fuente*.

Estado real:

- `Span.ns`/`StringView` son vistas insumo; la conversión array y string → slice ya está integrada
  al classifier y al lowering sin copia. Buffer (bloque crudo) pendiente.

## Gap

Falta la conversión sin copia de buffer → slice y las reglas de lifetime (destino ≤ fuente).
Los slices array→span y string→StringView quedaron cerrados.

## Nota

F02 depende de F01 (ranking) y es el lowering de la vista. No es una surface nueva; es la conversión
no-copy + el contrato mutability/lifetime.
