# NS304-T04-F07 — Ranking de method groups, overload priority y conversiones a fn*/fn&

- **Estado**: CERRADO (tipo natural) / residual (ranking multi-overload)
- **Rama**: development

## Resultado (validado)

- La inferencia de `var` comprueba `methodGroup` ANTES del chequeo de tipo (el typeKey de un
  method group está vacío), arma el `fn*` desde el overload único y lo convierte a delegado.
- **Prueba `TL-LANG-319`** (exit 42): `var f = Double; return f(21);` — **PASS**.

## Residual

- El ranking de MULTI-overload (elegir el overload con menos defaults, etc.) ya existe para
  target explícito; el caso `var` con varios overloads reporta ambigüedad (correcto). El ranking
  fino queda para testing final.
