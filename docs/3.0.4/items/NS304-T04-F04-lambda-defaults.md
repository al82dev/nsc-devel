# NS304-T04-F04 — Parámetros opcionales y defaults consteval en lambdas

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `Parser::parseLambdaExpression` (parámetros con default),
  `ProjectMethodBodyModel::bindLambdaToDelegate`, y la prueba `TL-LANG-317`. NO tocar CMake,
  toolchain, Builder, foreach, rango T01-F04.

## Auditoría (línea base)

- `parseParameter` ya soporta `int x = 5` (default) en delegados/métodos
  (TP-PAR-145: `delegate void Work(int first, long second = 42);`).
- Los parámetros de lambda SIN tipo contextuales (`(x, y)`) no admiten default; los que llevan
  tipo van por `parseParameter` (:13546) y en principio sí.
- `bindLambdaToDelegate` no valida/baja defaults de lambda contra el target (la firma del target
  `fn*`/delegate no lleva defaults en la identidad del tipo).

## Gap F04

Permitir `(int x = 5) => ...` en lambdas (y defaults consteval) y validar contra el target: el
default de la lambda no puede entrar en conflicto con el del delegate.

## Plan

1. Confirmar que el default se baja al `MethodLambdaBinding` (parámetro opcional + valor default).
2. Validar el default contra el delegate target y prohibir default en lambda no-convertida.
3. Prueba `TL-LANG-317`.

## Prueba puntual

`TL-LANG-317`: lambda `(int x = 5) => x + 1` convertida a delegate con default; invocación sin
argumento usa el default.
