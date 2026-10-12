# NS304-T04-F01 — Inferir tipo callable natural para lambda y method group

- **Estado**: EN IMPLEMENTACIÓN
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `ProjectMethodBodyModel.cpp` (análisis de `LambdaExpression` y
  `AnonymousLambdaType`), `ProjectGenericModel.cpp` (`inferLambdaTypeArguments`), y la prueba
  `TL-LANG-314`. NO tocar CMake, toolchain, Builder, foreach, rango T01-F04.

## Auditoría (línea base)

La infraestructura de callables ya es extensa (análisis por olas paralelas):
- `ProjectCallableAnalysisResult/Context/WaveScheduler/WaveReducer/BindingIndex`.
- `AnonymousLambdaType` (ProjectMethodBodyModel.cpp:5691) — firma de lambda anónima.
- `inferLambdaTypeArguments` (ProjectGenericModel.cpp:3164) — inferencia de T en lambda genérica.
- `analyzeExpression` con `case ast::AstNodeKind::LambdaExpression` (:9539).
- `functionType` (ProjectMethodBodyModel.cpp:7545) — construye el tipo `fn` desde un callable.

## Gap F01 (confirmado)

`bindLambdaToDelegate` (ProjectMethodBodyModel.cpp:22625) con `delegateTypeKey` vacío
(`inferAnonymous = true`, :22642) YA infiere el tipo anónimo cuando:
- todos los parámetros tienen tipo explícito (:22648-22670),
- y el retorno es explícito (`syntax->typeSyntax()`, :22671-22672).

**Falta**: inferir el TIPO DE RETORNO desde el cuerpo cuando no hay `-> T` explícito
(p.ej. `(int x) => x + 1` debe inferir `int`). Hoy `returnType` queda vacío en ese caso.

## Plan

1. Confirmar/implementar la inferencia de retorno desde el cuerpo del lambda anónimo
   (analizar el cuerpo y tomar su tipo) cuando no hay `-> T`.
2. Añadir/confirmar la inferencia de method group (rank único) a `fn*`/`fn&`.
3. Prueba `TL-LANG-314` (retorno explícito, ya existe) + extenderla al retorno inferido.

## Prueba puntual

`TL-LANG-314`: `var f = (int x) -> int => x + 1; return f(41);` (exit 42) — valida la
inferencia actual. Se extiende a `(int x) => x + 1` cuando se implemente el retorno inferido.
