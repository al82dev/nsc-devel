# NS304-T12-F06 — Generated members, colisiones, NSMETA e incremental

- **Estado**: COMPLETO — colisión de derive detectada + identidad/procedencia/cache/NSMETA verificados.
- **Rama**: development

## Implementado (2026-10-15)

- **Colisión de derive**: en `ProjectMetaprogrammingModel`, al construir los `derives_` se comprueba que
  cada miembro generado (`Clone`/`Equals`/`GetHashCode`/`ToString`/`operator==`/`operator!=`) no
  colisione con un miembro manual del mismo tipo (misma regla de spelling+scope). En caso de colisión
  se rechaza con: `derive '<trait>' generates member '<name>' which collides with an existing member
  in type '<identity>'`.

## Verificado O0

- `TL-LANG-521` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `record struct
  derive(Clone)` + `Clone()` manual es **rechazado**; el derive sin colisión compila y `Clone()` corre.

## Estado de las demás aristas (heredadas de T14)

| arista | estado | evidencia |
|---|---|---|
| identidad/procedencia canónica | ✅ | `generatedMemberIdentity` / `sameGeneratedIdentity` (`TL-LANG-501`); `originPath` en `GeneratedDeclaration` |
| cache/invalidación por identidad | ✅ | `comptimeCacheKey` + `deriveDependencyKey` (caps herméticos, no orden) |
| superficie NSMETA única | ✅ | NSMETA infraestructura interna sin versionado (`NS304-T08-F07`) |
| colisiones con backtrace | ✅ (colisión de derive) | `TL-LANG-521` |

## Contexto

- El hueco era asignar identidad/procedencia, detectar colisiones, cache por identidad y una sola
  superficie NSMETA. La identidad/cache/NSMETA ya estaban cerradas en T14; aquí se cerró la colisión
  de derive (la regla de mezcla por el mismo spelling+scope). El "backtrace" de macro se aplica a las
  colisiones del macro/mixin tipado de T14 (no al derive built-in, que no expande un macro literal).
