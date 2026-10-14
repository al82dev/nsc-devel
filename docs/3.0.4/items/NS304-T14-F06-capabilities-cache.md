# NS304-T14-F06 — Capacidades, inputs y cache

- **Estado**: AUDITADO (cache de compilación existe; capacidades/inputs del macro sin declarar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar capacidades, inputs y cache*.
Plan T14-F06 (DeepSeekDocs): *declarar archivos/config/variables permitidos; todo input entra al cache
key; sin reloj, red, cwd implícito ni estado global; mismos inputs producen AST/metadata/diagnósticos
idénticos en build limpio/incremental*.

Estado real:

- La cache/build graph de compilación existe; el macro no declara sus capacidades/inputs (archivos/
  config/variables) ni todo entra al cache key.

## Gap

Falta declarar las capacidades/inputs del macro + la cache key completa (sin reloj/red/cwd/estado
global) + la equivalencia clean/incremental (mismos inputs → mismos AST/metadata/diagnósticos).

## Nota

F06 depende de F02 (generación) + la cache existente. No es una surface nueva; es el determinismo de
inputs del macro.
