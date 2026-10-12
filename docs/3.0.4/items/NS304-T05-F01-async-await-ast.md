# NS304-T05-F01 — Retirar rechazo NS3049 de async/await y completar AST/recovery

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (línea base)

- `async`/`await` están RECHAZADOS por el parser vía `reportDeferredLanguage3(..., Language3Feature::AsyncAwait)`
  en varios puntos: Parser.cpp:2560, :3367, :5236, :5310, :5634, :6207 y `await` en :13729.
- El diagnóstico es `NS3049DeferredLanguageSurface` (Parser.cpp:198-208).

## Gap F01

1. Retirar el rechazo `NS3049` de `async` (prefijo de lambda/función) y de `await` (expresión).
2. Completar el AST: `await` como expresión (`AwaitExpression`), `async` como modifier de callable,
   y recovery ante `async`/`await` malformados.

## Plan

1. Sustituir los `reportDeferredLanguage3(AsyncAwait)` por parseo real de `await` (expresión unaria
   de primer nivel) y `async` (modifier), o por un error más específico que preserve recovery.
2. Prueba `TL-LANG-322`.

## Prueba puntual

`TL-LANG-322`: parseo de `async`/`await` sin el rechazo NS3049 (AST/recovery), aunque la
transformación productiva (F03) aún no baje.
