# NS304-T14-F06 — Capacidades, inputs y cache

- **Estado**: COMPLETO — cache key completa y hermética (sin reloj/red/cwd/estado global).
- **Rama**: development

## Implementado (2026-10-15)

- `db700020` — cierre del hueco auditado:
  - **Cache key completa**: `comptimeCacheKey(capabilities, inputs, sourceDigest)` produce la clave
    determinista `comptime-cache-v1|caps=<herméticas ordenadas>|<inputs identity>|source=<digest>`.
  - **Sin estado no reproducible**: sólo las capacidades herméticas (`ReadDeclaredFile`,
    `ReadDeclaredEnvironment`) entran en la clave; `Clock`/`Random`/`Ffi` se excluyen, de modo que
    clean e incremental comparten la misma clave y el reloj/red/cwd/estado global no la alteran.
  - Los inputs se encodifican por el `comptimeInputIdentity` existente (orden por identidad,
    digest por contenido).

## Verificado O0

- `TL-LANG-502` (CL-LANG-014, Host.Language, `nsc_component_tests`): PASS — clave determinista,
  exclusión de capacidades no herméticas, y forma vacía `caps=0,1,|comptime-input-v1||source=s`.
- Unit `nsc_ns3_t11_metaprogramming_tests`: PASS (17 checks).

## Contexto

- El hueco era declarar capacidades/inputs del macro + la cache key completa + la equivalencia
  clean/incremental. La cache/build graph y el `reproducibilityDigest` ya existían; aquí se cerró la
  clave determinista sin estado global. La declaración sintáctica de capacidades/inputs en el macro
  (superficie `#macro`/comptime) pertenece a T12-F06 y no se sustituyó por ABI inventado.
