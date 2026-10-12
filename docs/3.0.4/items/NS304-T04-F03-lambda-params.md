# NS304-T04-F03 — Parámetros ref/ref readonly/in/out/scoped en lambdas

- **Estado**: EN IMPLEMENTACIÓN
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `Parser::parseLambdaExpression` (parámetros),
  `ProjectMethodBodyModel::bindLambdaToDelegate` (validación de parámetros), y la prueba
  `TL-LANG-316`. NO tocar CMake, toolchain, Builder, foreach, rango T01-F04.

## Auditoría (línea base)

Los parámetros de lambda hoy exigen tipo explícito por valor; `bindLambdaToDelegate`
(ProjectMethodBodyModel.cpp:22659-22666) RECHAZA `ref`/`out`/`in` con "an unconverted lambda
parameter must be passed by value; ref/out/in require a target function type".

## Gap F03

Permitir `ref` / `ref readonly` / `in` / `out` / `scoped` en parámetros de lambda, con el tipo
inferido del TARGET (`fn*`/delegate) en vez de repetirlo. `ref`/`ref readonly`/`in` deben ser
alias (con mutabilidad), `out` debe exigir asignación antes del retorno, `scoped` no escapa.

## Plan

1. Parser: aceptar `ref`/`ref readonly`/`in`/`out`/`scoped` delante del nombre del parámetro sin
   tipo explícito (cuando el lambda tiene destino tipado).
2. Sema: bajar el modo del parámetro al `MethodLambdaBinding` y validar `out` (definite
   assignment) y `scoped` (escape).
3. Prueba `TL-LANG-316`.

## Prueba puntual

`TL-LANG-316`: lambda con `(ref x, out y)` contra un `fn*` tipado; `out` no asignado debe diagnosticar.
