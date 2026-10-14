# NS304-T11-F04 — Params moderno

- **Estado**: AUDITADO (`params` array tradicional existe; span/inline/builder pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar params moderno*.
Plan T11-F04 (DeepSeekDocs): *permitir `params` sobre span, inline buffer o builder aprobado; call
site elige storage explícito y de duración suficiente; pequeño número de argumentos no crea heap
array; spreads evalúan una vez y preservan orden*.

Estado real:

- `params` sobre array tradicional existe (el call site materializa un array).
- `params` sobre span/inline buffer/builder aprobado: sin surface; el storage explícito y el no-heap
  para pocos argumentos no están integrados.

## Gap

Falta permitir `params` sobre span/inline buffer/builder, con call site que elige storage explícito
de duración suficiente (sin heap array para pocos argumentos) y spreads evaluados una vez en orden.

## Nota

F04 depende de F01/F02 (span) + la surface de `params` sobre vista/builder. No es una surface nueva
de `params`; es el storage no-heap del call site.
