# NS304-T04-F06 — Funciones locales genéricas, recursión y autoreferencia segura

- **Estado**: CERRADO (funciones locales + recursión) / residual (genéricas)
- **Rama**: development

## Resultado

- Parser: `type name (...)` baja a lambda natural (`parseLocalFunctionAsLambda`, return unico como
  cuerpo de expresion). `TL-LANG-325` (Double(21)=42) PASA.
- Recursión: `bindLambdaToDelegate` registra `local-function:<name>` en el scope del lambda con el
  TIPO ANONIMO de lambda (`anonymousLambdaTypeIdentity`), de modo que el lowering resuelve
  `directLambdaName` y el lambda se auto-llama. `TL-LANG-326` (Fact(5)=120) PASA.
- **Pendiente**: funciones locales GENÉRICAS (`int<T> f(...)`) — el lowering actual no consume la
  lista de parámetros genéricos.
