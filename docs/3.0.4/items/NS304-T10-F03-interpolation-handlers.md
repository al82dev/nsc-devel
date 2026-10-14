# NS304-T10-F03 — Interpolation handlers productivos

- **Estado**: IMPLEMENTADO (handler `AppendLiteral`/`AppendFormatted`/`Finish` productivo, surface A)
- **Rama**: development

## Surface decidida

- Decisión de surface #12 (A): el target de la interpolación es un tipo handler con los miembros
  `AppendLiteral(string)` + `AppendFormatted(<hole>)` + `Finish()`.

## Implementación

- Parser: `InterpolatedStringExpression` retiene los hole-AST (cada `{...}` parseado una vez, en
  orden de fuente) como hijos del nodo, tanto en la forma estática como en la runtime.
- Sema: si `activeTargetTypeKey_` es un tipo con `AppendLiteral`+`AppendFormatted`+`Finish`, valida
  la forma (`AppendFormatted` con un parámetro), analiza cada hueco una vez y emite
  `MethodCoreOperationKind::InterpolationHandler` (ordinals de los tres miembros en
  `builderMemberOrdinals`).
- HIR: `BlockExpression` → temporal handler (DefaultValue) + `AppendLiteral(parte)` +
  `AppendFormatted(hueco)` en orden izquierda→derecha + `Finish()`, sin concatenación temporal.
- MIR/LLVM: los `DirectInstanceCall` existentes bajan las llamadas; el temporal es un struct local.

## Prueba

- `TL-LANG-474` — `struct Builder{int Sum; AppendLiteral; AppendFormatted(int); Finish()}`;
  `Builder b = $"1 {2} 3"; b.Finish()` → exit 2 (el hueco `2` se suma sin concatenar strings).

## Nota (residual)

- La superficie A admite `AppendFormatted` **no-genérico** (overload por tipo del hueco). El
  `AppendFormatted<T>` **genérico** (huecos de tipo arbitrario vía especialización de método) queda
  como ampliación: hoy `findMembers("AppendFormatted")` no ve el template (vive en
  `ProjectGenericModel.templates`, no en `memberModel.members()`). El protocolo productivo en sí
  (literal + hueco + finish, sin heap) está cerrado.

## Commits

- `7dcd82d5` — handler productivo `AppendLiteral`/`AppendFormatted`/`Finish` (TL-LANG-474).
