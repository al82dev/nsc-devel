# Tanda universal 11: `match` contextual sin secuestrar llamadas

## Objetivo

Corregir el parser para que `match` conserve su doble contrato contextual:

- `match (value) { ... }` inicia el constructo de selección del lenguaje.
- `match(value)` sigue siendo una llamada válida cuando `match` es un identificador.

## Causa confirmada

El parser seleccionaba `parseMatchStatement` o `parseMatchExpression` al encontrar
únicamente el prefijo `match (`. Ese prefijo también pertenece a una llamada normal,
por lo que un parámetro delegado llamado `match` producía diagnósticos falsos sobre
brazos, patrones y `=>`.

## Implementación

- Se agregó un lookahead iterativo que encuentra el cierre del grupo parentizado.
- El constructo contextual sólo se selecciona cuando después del cierre aparece `{`.
- El lookahead admite paréntesis anidados y no usa recursión.
- Las rutas de statement y expression comparten la misma decisión.

## Archivos

- `include/nativesharp/Parser/Parser.h`
- `src/Parser/Parser.cpp`

## Contratos preservados

- No cambia el AST ni el lowering de un `match` real.
- No cambia la resolución semántica de llamadas ordinarias.
- No se modifica NSMETA, NSLink, ownership ni la instalación oficial en `C:`.
- No se relajan verificadores ni se reescriben expectativas para ocultar el defecto.

## Validación dirigida

- `nsc.stage52.generic-collections-compiler-fixes`: **PASSED**, incluida la llamada
  cross-project al delegado cuyo parámetro se llama `match`.
- `nsc.ns3.t07.errors`: la comprobación existente que ejecuta
  `match(throws())` dejó de fallar. La prueba completa conserva siete fallos ajenos
  a esta tanda: seis expectativas antiguas de rechazo por efectos `throws` y una
  expectativa textual sobre MIR 1.29.

No se ejecuta baseline, certificación, promoción ni matriz O1/O2/O3.
