# NS304-T05-F01 — Retirar rechazo NS3049 de async/await y completar AST/recovery

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (línea base)

- `async`/`await` están RECHAZADOS por el parser vía `reportDeferredLanguage3(..., Language3Feature::AsyncAwait)`
  en varios puntos: Parser.cpp:2560, :3367, :5236, :5310, :5634, :6207 y `await` en :13729.
- El diagnóstico es `NS3049DeferredLanguageSurface` (Parser.cpp:198-208).

## Resultado (validado)

- `await expr` → `AwaitExpression` (nuevo `AstNodeKind`) ✅ (`2ab2ff03`, `c6e4f22c`).
- `async` modifier: NS3049 retirado en los 6 puntos (consume) y guardado como marcador "async" en
  el `FunctionDeclaration` global (`f184bb92`, `d25d16cf`).
- 13/13 pruebas sin regresión.

## Residual

- Bajar el marcador "async" a class/interface METHOD (los 5 puntos restantes solo consumen; falta
  el almacenamiento en `parseMethodTail`). El state machine productivo es F03.
