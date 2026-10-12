# NS304-T04-F02 — Retorno explícito, atributos, calling convention y efectos en lambdas

- **Estado**: EN IMPLEMENTACIÓN
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `Parser::parseLambdaExpression`/`looksLikeLambdaExpression`,
  `ProjectMethodBodyModel::bindLambdaToDelegate`, `ProjectNamespaceFunctionModel`/`ProjectMemberModel`
  si se necesitan campos nuevos, y la prueba `TL-LANG-315`. NO tocar CMake, toolchain, Builder,
  foreach, rango T01-F04.

## Auditoría (línea base)

El grammar de lambda actual (ParserLambdaContinuationModule/LookaheadModule) soporta:
- `x => x`, `(int x) => x`, `(int x) -> int => x` (retorno explícito), `[]() => 0` (modos de captura),
  `<T>(T v) where T : struct, copy => v` (genérico).

NO soporta (TP-PAR-451 lo confirma: "no async/unsafe/noheap prefix flags"):
1. **Atributos** sobre lambda.
2. **Calling convention** (`extern C`, etc.) sobre lambda.
3. **Efectos** `noheap`/`unsafe` (y `async`/`throws` se tratan en T05/T07, no aquí).

## Gaps F02 (orden)

1. Efectos `noheap`/`unsafe` como prefijos de lambda (reconocer en `looksLikeLambdaExpression` +
   `parseLambdaExpression`, propagar a `CallableTrait`/`AnonymousLambdaType`).
2. Atributos sobre lambda (parsear `[...]` delante y adjuntar al `LambdaExpression`).
3. Calling convention `extern C` sobre lambda (propagar al `FunctionType`/`CallingConvention`).

## Plan

1. Reconocer `noheap`/`unsafe` como prefijos de lambda (y rechazar `async` con diagnóstico hasta T05).
2. Bajar el flag a `CallableTrait`/binding y al tipo `fn`.
3. Atributos y calling convention si el pipeline de `TypeSyntax`/`FunctionType` ya los admite.

## Prueba puntual

`TL-LANG-315`: lambda `noheap`/`unsafe` compila y conserva el efecto en el tipo; lambda con
atributo y `extern C` si procede.
