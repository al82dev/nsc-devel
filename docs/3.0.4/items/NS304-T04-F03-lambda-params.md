# NS304-T04-F03 — Parámetros ref/ref readonly/in/out/scoped en lambdas

- **Estado**: CERRADO (ref/out/in) / `ref readonly`+`scoped` diferidos a T11-F07
- **Validación secuencial**: confirmado en campaña; `TL-LANG-316` PASS + guards
  `TL-LANG-314/317/318/319/320/327` PASS (2026-10-13). Avanza a 33/147.
- **Rama**: development

## Resultado (validado)

- Parser reconoce `(ref x, out y, in z)` sin tipo y guarda `pass:ref/out/in` en el auxiliaryText
  del `ParameterDeclaration` (mismo formato que `parseParameter`).
- `bindLambdaToDelegate` ya cotejaba `lambdaPass` contra `delegatePass` (22791-22814) e infería el
  tipo contextual del target; con el prefijo `pass:` el cotejo y la inferencia funcionan.
- **Prueba `TL-LANG-316`** (exit 11): `Bump bump = (ref value) => { value = value + 1; };` — **PASS**.

## Nota (deferido)

- `ref readonly` y `scoped ref` son `GAP_OBLIGATORIO` de **NS304-T11-F07** (ref fields y scoped ref),
  no de T04-F03 (ver `NS304-T00-F03-csharp-8-14-matrix.md`). Se cierran ahí.

