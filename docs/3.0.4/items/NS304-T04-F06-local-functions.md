# NS304-T04-F06 — Funciones locales genéricas, recursión y autoreferencia segura

- **Estado**: EN IMPLEMENTACIÓN (parser ✅, Sema pendiente)
- **Rama**: development

## Resultado parcial

- Parser: `type name (...)` al principio de una sentencia ahora parsea como función local
  (`looksLikeLocalFunction` + `parseFunctionOrGlobalVariable`, `cf7889cf`). `int Fact(int n){...}`
  ya parsea.
- **Falta Sema**: `Fact(5)` no resuelve ("unknown identifier 'Fact'"). Hay que registrar la función
  local en el scope (nombre → callable) y analizar su cuerpo en la ola de callables, como una
  lambda NOMBRADA (captura + recursión).

## Plan

1. En `analyzeStatement`/`analyzeBlock`, manejar `FunctionDeclaration` dentro de un bloque:
   registrar el nombre en el scope antes de analizar el cuerpo (autoreferencia) y bajar un
   `MethodLambdaBinding`/callable local.
2. Resolver `Fact(5)` como llamada a la función local.
3. Prueba `TL-LANG-325` (ya redactada; se re-registra al pasar).
