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

## Gap F01 (a confirmar)

Determinar si el tipo callable NATURAL (`fn*(params) -> ret` / `fn&(...)`) se infiere cuando
la lambda/method group es el único candidato (sin contexto de asignación), o si hoy exige
`var`/asignación a un delegate explícito. Si la inferencia ya existe, F01 se cierra con una
prueba; si no, se añade el camino de inferencia.

## Plan

1. Verificar el camino de inferencia para lambda sin destino tipado.
2. Añadir/confirmar la inferencia de method group (rank único) a `fn*`/`fn&`.
3. Prueba `TL-LANG-314`: lambda y method group infieren tipo callable natural y se invocan.

## Prueba puntual

`TL-LANG-314`: asignar lambda a `var`, invocarla, y convertir un method group unívoco.
