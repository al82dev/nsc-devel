# NS3-T03-F05 — Pattern guards and narrowing

## Estado

`EN CURSO`. Los guards son productivos y su contrato de ejecución/cobertura está cerrado. Queda
pendiente el refinamiento flow-sensitive de nullability e intervalos sobre nombres existentes.

## Qué se hizo

- `pattern when condition =>` se acepta en brazos de `match`.
- El guard baja dentro del scope creado por el pattern y después de sus pruebas estructurales.
- Un guard falso continúa determinísticamente con el siguiente brazo.
- Sólo `when true` conserva cobertura; cualquier otro guard marca el brazo como parcial.
- Un wildcard guardado ya no cierra incorrectamente un dominio abierto.
- Los rangos guardados no bloquean como solapamiento a un rango posterior que cubre el camino
  donde el guard falla.
- `when false` recibe diagnóstico de inalcanzabilidad y los guards no booleanos reutilizan la
  validación semántica de condiciones.
- La gramática termina antes del `=>`, evitando confundir un identificador guard con una lambda.

## Archivos modificados

- `src/Parser/Parser.cpp`: parse, orden de evaluación, lowering y cobertura conservadora.
- `src/Frontend/ProjectMethodBodyModel.cpp`: decodificación de wildcard guardado y cierre correcto
  de dominios abiertos.
- `CMakeLists.txt`: target dirigido exclusivo del ítem.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T03PatternGuardTests.cpp`: ejecución O0/NSLink, side effects, orden,
  bindings, fall-through, tipos, cobertura abierta/cerrada y constantes.
- `docs/3.0/language/pattern-guards.md`: contrato bilingüe para usuarios y herramientas.
- Este documento: evidencia atómica y deuda explícita del ítem.

## Mejora obtenida

Los casos estructuralmente iguales pueden separarse por condiciones sin anidar `if` dentro del
brazo ni duplicar extracción de payload. La exhaustividad permanece segura: ningún guard dinámico
promete cubrir valores para los que podría devolver falso.

## Pendiente exacto

- Una tabla compartida de hechos de flujo para `x != null`, caso variant activo e intervalos.
- Consumo de esos hechos por resolución de miembros, eliminación de checks y dumps HIR/MIR.
- Pruebas de join/invalidation después de asignaciones y llamadas; no se declarará `TERMINADA`
  hasta que esos hechos afecten semántica productiva y no sólo documentación.

## Validación dirigida

- `nsc_ns3_t03_pattern_guard_tests`: 6/6 con O0 y NSLink local.
- `nsc_ns3_t03_exhaustive_match_tests`: 9/9.
- `nsc_ns3_t03_fundamental_pattern_tests`: 6/6.
- `nsc_ns3_t03_conditional_pattern_tests`: 6/6.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
