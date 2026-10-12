# NS304-T04-F03 — Parámetros ref/ref readonly/in/out/scoped en lambdas

- **Estado**: EN IMPLEMENTACIÓN (ref/out/in validados)
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `Parser::parseLambdaExpression` (parámetros),
  `ProjectMethodBodyModel::bindLambdaToDelegate` (validación de parámetros), y la prueba
  `TL-LANG-316`. NO tocar CMake, toolchain, Builder, foreach, rango T01-F04.

## Resultado (validado)

- Parser reconoce `(ref x, out y, in z)` sin tipo y guarda `pass:ref/out/in` en el auxiliaryText
  del `ParameterDeclaration` (mismo formato que `parseParameter`).
- `bindLambdaToDelegate` ya cotejaba `lambdaPass` contra `delegatePass` (22791-22814) e infería el
  tipo contextual del target; con el prefijo `pass:` el cotejo y la inferencia funcionan.
- **Prueba `TL-LANG-316`** (exit 11): `Bump bump = (ref value) => { value = value + 1; };` — **PASS**.

## Pendiente

- `ref readonly` (dos palabras) y `scoped` (lifetime, no pass-mode): no reconocidos aún.
- Semántica de `out` (definite assignment) y `in` (readonly) no se validan por separado aquí.

