# NS304-T03-F05 — Usefulness/exhaustiveness para longitud, OR-pattern y dominios abiertos

- **Estado**: EVIDENCIA TÉCNICA ADELANTADA COMPLETA; cierre secuencial pendiente.
- **Rama**: development
- **Contabilidad**: no incrementa el 25/147 vigente; requiere cierre de F04 y luego su propio
  cierre en orden.
- **Alcance (SCOPE LOCK)**: `Language/SumTypeSemantics.*`, `ProjectMethodBodyModel.cpp` (puntos
  de reporte en 4383/8591), y la prueba `TL-LANG-312`. NO tocar CMake, toolchain, Builder,
  foreach, rango T01-F04, ni los commits de F03/F04.

## Auditoría (línea base)

El checker actual (`analyzeClosedMatch`, SumTypeSemantics.cpp:74) cubre SOLO **dominios
cerrados** (variantes/enums):

- `covered` = conjunto de casos cubiertos por los brazos.
- `unguardedWildcard` = si hay `_` sin guard.
- `useful` por brazo = hay casos aún no cubiertos (o `_` útil).
- `missingCases` = dominio − cubierto (si no hay `_`).
- `exhaustive = unguardedWildcard || missingCases.empty()`.

El reporte "non-exhaustive match; missing variant case" está en ProjectMethodBodyModel.cpp:4383
y 8591.

## Gaps F05

1. **Longitud (list patterns)**: `[a]` y `[a, b]` no cubren toda longitud; falta razonar sobre
   la longitud del patrón (exacta vs con rest) para declarar exhaustividad/alcance.
2. **OR-pattern**: `a | b` ya se parsea y se valida el binding idéntico (line 55-72), pero el
   análisis de exhaustividad no expande alternativas para calcular cobertura compuesta.
3. **Dominios abiertos**: `int`/`string`/literales son infinitos → sin `_` deben declararse
   no-exhaustivos (hoy el checker solo se invoca para variantes cerradas).

## Plan

1. Modelar cobertura de list pattern por longitud (exacta/≥ con rest) y composición con `|`.
2. Generalizar `analyzeClosedMatch` a dominios abiertos: si el dominio no es cerrado, la única
   forma de exhaustividad es un `_` (o `or` que lo implique).
3. Integrar OR-pattern expandiendo alternativas antes del cálculo de cobertura.
4. Reportar missing cases de longitud y de OR en los puntos 4383/8591.

## Prueba puntual

`TL-LANG-312`: match no-exhaustivo sobre `int` (dominio abierto) debe diagnosticar; list
`[a]`/`[a, b]` sin `_` debe diagnosticar missing length; OR-pattern útil/unreachable.

## Evidencia de cierre

- `TL-LANG-267` valida directamente el modelo: unión de longitudes exactas, unión
  exacta+rest exhaustiva y redundancia cuando todos los dominios de un brazo OR ya están
  cubiertos. `PASS`, 196 us, 2026-10-13.
- `TL-LANG-312` valida la ruta Parser→Frontend→Sema: `[x] | [x, y]` conserva ambas
  longitudes; `[] | [_, ..]` cubre el dominio; exacto|rest redundante se diagnostica; un
  dominio abierto sin wildcard se rechaza. `PASS`, 468,431 us, 2026-10-13.
- Construcción focal: `nsc` 7/7 pasos y `nsc_component_tests` 2/2 pasos. No se ejecutó la
  batería completa.
