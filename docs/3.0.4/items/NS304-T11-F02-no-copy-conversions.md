# NS304-T11-F02 — Conversiones sin copia

- **Estado**: AUDITADO (vistas insumo; conversión sin copia no integrada)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar conversiones sin copia*.
Plan T11-F02 (DeepSeekDocs): *array, buffer, string y slice producen base/offset/length/provenance;
nunca array temporal; inmutable puede aceptar fuente mutable sin conceder escritura; mutable exige
storage mutable; lifetime destino no excede fuente*.

Estado real:

- `Span.ns`/`StringView` son vistas insumo; la conversión array/buffer/string → slice no está
  integrada al classifier (F01) ni al lowering como `{base, offset, length, provenance}`.

## Gap

Falta la conversión sin copia de array/buffer/string → slice/span (base+offset+length+provenance, sin
array temporal), y las reglas de mutabilidad (inmutable acepta mutable sin escritura; mutable exige
storage mutable) + lifetime (destino ≤ fuente).

## Nota

F02 depende de F01 (ranking) y es el lowering de la vista. No es una surface nueva; es la conversión
no-copy + el contrato mutability/lifetime.
