# NS304-T10-F03 — Interpolation handlers productivos

- **Estado**: AUDITADO (interpolación consteval F04 + raw F02 cerradas; handler productivo sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar interpolation handlers productivos*.
Plan T10-F03 (DeepSeekDocs): *definir protocolo de construcción, `AppendLiteral`,
`AppendFormatted`, finish y failure sobre stack/writer/custom sink; selección por overload y
constraints; evaluación una vez, izquierda a derecha; no concatenar strings temporalmente ni asignar
salvo decisión explícita del sink*.

Estado real:

- **Interpolación raw** (F02): delimitadores + braces + indent trimming cerrados.
- **Formato consteval** (F04): placeholders/opciones validados en compilación cerrados.
- **Handler productivo**: sin surface; la interpolación baja a una string materializada (no a un
  handler `AppendLiteral`/`AppendFormatted`).

## Gap

Falta el protocolo de construcción (handler) que recibe los huecos sin concatenar strings
temporalmente: `AppendLiteral`/`AppendFormatted`/finish/failure sobre stack/writer/custom sink.

### Verificación (2026-10-14)

- `Builder b = $"{1}"` da `cannot initialize local 'b' of type 'Builder' from 'string-literal'`: la
  interpolación materializa una string (no baja a `AppendLiteral`/`AppendFormatted`). El tramo es
  análogo al builder protocol de T09-F05 (core-op + `BlockExpression` + detección del target por
  `AppendLiteral`/`AppendFormatted`/finish), reutilizando esa maquinaria.

## Decisión de surface (material, para Alberto)

- **(A) handler por tipo target + `AppendLiteral`/`AppendFormatted`** (recomendada): el target de la
  interpolación es un builder/handler con esos miembros.
- **(B) `IInterpolationHandler<T>` interface explícita**.

Recomendación: **(A)**.

## Nota

F03 depende de la surface de handler (A/B) y de F04 (consteval) ya cerrado. El handler debe evaluar
los huecos una vez, izquierda a derecha, sin concatenación temporal ni heap salvo decisión del sink.
