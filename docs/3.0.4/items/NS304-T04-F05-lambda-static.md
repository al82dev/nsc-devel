# NS304-T04-F05 — Lambdas y funciones locales estáticas + prohibición de captura

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `Parser::parseLambdaExpression`/`looksLikeLambdaExpression`,
  `ProjectMethodBodyModel::bindLambdaToDelegate`/análisis de captura, y la prueba `TL-LANG-318`.
  NO tocar CMake, toolchain, Builder, foreach, rango T01-F04.

## Auditoría (línea base)

- El grammar de lambda NO tiene prefijo `static` (solo `noheap`/`unsafe`/`extern C` ya añadidos
  en F02, más la lista de captura `[...]`).
- No hay `parseLocalFunction` ni "función local estática" con prohibición de captura explícita;
  los `staticLocal` del pipeline son VARIABLES locales estáticas (storage module-static), no lambdas.
- La captura de lambda ya se analiza (`binding.captures`, `closureCaptures`), y la lista de
  captura `[]` vacía (`[]() => ...`) ya existe como "sin captura".

## Gap F05

1. Prefijo `static` en lambda: `static x => x` = lambda sin captura (prohibir toda captura).
2. Función local `static int f() {...}` dentro de un método (sin captura del entorno).
3. Prohibición de captura: un lambda/función local `static` que intente capturar debe diagnosticar.

## Plan

1. Parser: reconocer `static` como prefijo de lambda (y de función local) + almacenar el flag.
2. Sema: `static` fuerza `closureCaptures` vacío y diagnostica cualquier captura.
3. Prueba `TL-LANG-318`.

## Prueba puntual

`TL-LANG-318`: `static` lambda sin captura compila; un `static` lambda que captura un local
diagnostica.
