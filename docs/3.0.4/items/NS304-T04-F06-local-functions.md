# NS304-T04-F06 — Funciones locales genéricas, recursión y autoreferencia segura

- **Estado**: CERRADO
- **Validación secuencial**: confirmado en campaña; `TL-LANG-325/326/352` PASS + guards
  `TL-LANG-318/351` PASS (2026-10-13). Avanza a 36/147.
- **Rama**: development

## Resultado

- Parser: `type name (...)` baja a lambda natural (`parseLocalFunctionAsLambda`, return unico como
  cuerpo de expresion). `TL-LANG-325` (Double(21)=42) PASA.
- Recursión: `bindLambdaToDelegate` registra `local-function:<name>` en el scope del lambda con el
  TIPO ANONIMO de lambda (`anonymousLambdaTypeIdentity`), de modo que el lowering resuelve
  `directLambdaName` y el lambda se auto-llama. `TL-LANG-326` (Fact(5)=120) PASA.
- Genéricas: `looksLikeLocalFunction` acepta un identificador desnudo como tipo de retorno y
  `parseLocalFunctionAsLambda` copia los parámetros genéricos al lambda. `TL-LANG-352`
  (`T Identity<T>(T x)`) PASA.
