# NS304-T04-F01 — Inferir tipo callable natural para lambda y method group

- **Estado**: CERRADO (lambda + method group natural)
- **Validación secuencial**: confirmado en campaña; `TL-LANG-314` PASS, `TL-LANG-319` PASS
  (2026-10-13); guard `TL-LANG-315` (T03) PASS. Avanza a 31/147.
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

## Resultado (validado)

`bindLambdaToDelegate` (ProjectMethodBodyModel.cpp:22625) con `delegateTypeKey` vacío
(`inferAnonymous = true`, :22642) infiere el tipo callable natural COMPLETO:

- todos los parámetros deben tener tipo explícito (:22648-22670);
- el cuerpo se analiza (:22896-22899) y, si el retorno NO es explícito, se **infiere del cuerpo**
  (:22904-22908: `returnType = body.typeKey`);
- la clave anónima se arma con ese retorno (:22954-22958).

**Prueba `TL-LANG-314`** (exit 42): `var f = (int x) => x + 1; return f(41);` — **PASS**, valida la
inferencia del retorno desde el cuerpo.

## Cierre (method group natural)

Inferencia de **method group** (rank único) a `fn*`/`fn&` sin destino tipado explícito, validada
por `TL-LANG-319` (`var f = Double; return f(21);` → 42). La conversión method group → delegate la
provee `bindMethodGroupToDelegate` (:22974) y el tipo `fn` natural único la resuelve el ranking de
T04-F07. Sin residual pendiente.
