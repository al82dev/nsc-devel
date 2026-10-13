# NS304-T04-F07 — Ranking de method groups, overload priority y conversiones a fn*/fn&

- **Estado**: PARCIAL; ABIERTO (ranking multi-overload)
- **Rama**: development

## Resultado (validado)

- La inferencia de `var` comprueba `methodGroup` ANTES del chequeo de tipo (el typeKey de un
  method group está vacío), arma el `fn*` desde el overload único y lo convierte a delegado.
- **Prueba `TL-LANG-319`** (exit 42): `var f = Double; return f(21);` — **PASS**.
- Esa prueba certifica sólo el caso de overload único; no cierra F07.

## Residual

- El ranking de MULTI-overload (elegir el overload con menos defaults, etc.) existe para algunos
  targets explícitos, pero no cuenta con una prueba puntual que congele prioridad, desempate y
  conversiones a `fn*`/`fn&` entre candidatos.
- El caso `var` con varios overloads reporta ambigüedad; esto no sustituye la matriz de ranking
  exigida por F07. El ítem permanece abierto hasta disponer de esa evidencia.
