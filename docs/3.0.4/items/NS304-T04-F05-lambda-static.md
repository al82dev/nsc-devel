# NS304-T04-F05 — Lambdas y funciones locales estáticas + prohibición de captura

- **Estado**: CERRADO
- **Rama**: development

## Resultado (validado)

- Parser conserva `KwStatic` como modificador estructurado tanto en lambdas escritas directamente
  como en la lambda sintetizada por una función local; los clones Parser, Generic y Opaque
  preservan la lista de modificadores.
- `bindLambdaToDelegate` consulta exclusivamente `hasModifier(KwStatic)`: nombres de funciones,
  variables capturadas, efectos y contratos auxiliares que contengan el texto `static` no alteran
  la semántica.
- Una lambda o función local `static` sin captura es válida. Una captura real emite el diagnóstico
  específico `a static local function cannot capture variables from the enclosing scope` para la
  forma de función local.
- **Evidencia focal (2026-10-13)**: `TL-LANG-318`, `TL-LANG-351`, `TL-LANG-352`, `TL-LANG-355` y
  el guard de closure ordinaria `TL-LANG-320` — **PASS**. `TL-LANG-318` incluye una captura
  ordinaria llamada `staticBias`; `TL-LANG-351` incluye la función ordinaria `staticNamed`; y
  `TL-LANG-355` exige fallo de compilación más el diagnóstico exacto.
