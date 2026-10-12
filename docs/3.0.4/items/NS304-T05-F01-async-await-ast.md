# NS304-T05-F01 — Retirar rechazo NS3049 de async/await y completar AST/recovery

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (línea base)

- `async`/`await` están RECHAZADOS por el parser vía `reportDeferredLanguage3(..., Language3Feature::AsyncAwait)`
  en varios puntos: Parser.cpp:2560, :3367, :5236, :5310, :5634, :6207 y `await` en :13729.
- El diagnóstico es `NS3049DeferredLanguageSurface` (Parser.cpp:198-208).

## Resultado parcial (validado)

- `await expr` ya parsea en `AwaitExpression` (nuevo `AstNodeKind`); el rechazo NS3049 de `await`
  está retirado (`2ab2ff03`, `c6e4f22c`). Sin regresión (13/13).

## Pendiente

- `async` como MODIFIER de función/lambda (los 6 puntos de rechazo NS3049 restantes: :2560,
  :3367, :5234, :5310, :5634, :6207).
- **Nota**: `async` NO puede volverse keyword reservada (`KwAsync`) porque también es un EFECTO de
  tipo función (`fn*()->void async`, Parser.cpp:1226 usa `atContextualKeyword("async")`). El
  modifier debe guardarse como marcador en el `auxiliaryText` del callable, sin reservar la
  palabra.
- El state machine productivo es F03.
