# NS304-T11-F04 — Params moderno

- **Estado**: PARCIAL (`params ReadOnlySpan<T>` ✅ con O0; inline buffer y builder aprobado pendientes)
- **Rama**: development

## Resultado (span — 2026-10-14)

- `params ReadOnlySpan<T>` ya es productivo y sin heap: `Sum(params ReadOnlySpan<int> values)` +
  `Sum(3, 4)` baja a una vista prestada (no array heap). El `params Span<T>` mutable se rechaza a
  propósito ("implicit heap arrays are forbidden") y el inline array por valor también
  ("cannot be passed by value").
- Prueba puntual **TL-LANG-411** (exit 7) registrada. Guards span PASS.

## Residual

- **inline buffer** (`params` sobre array fijo por referencia) y **builder aprobado** (`params` sobre
  un builder tipo collection). Storage explícito de duración suficiente + spreads.

## Auditoría (línea base)

Contrato (tracking): *Implementar params moderno*.
Plan T11-F04 (DeepSeekDocs): *permitir `params` sobre span, inline buffer o builder aprobado; call
site elige storage explícito y de duración suficiente; pequeño número de argumentos no crea heap
array; spreads evalúan una vez y preservan orden*.

Estado real:

- `params ReadOnlySpan<T>` existe y es sin heap. El `params` sobre array tradicional materializa un
  array. Inline buffer y builder aprobado sin surface.

## Gap

Falta `params` sobre inline buffer (array fijo por referencia) y builder aprobado, con storage
explícito sin heap y spreads evaluados una vez en orden. El slice span quedó cerrado con O0.

## Nota

F04 depende de F01/F02 (span) + la surface de `params` sobre vista/builder. No es una surface nueva
de `params`; es el storage no-heap del call site.
