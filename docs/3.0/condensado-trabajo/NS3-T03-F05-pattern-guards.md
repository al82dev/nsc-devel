# NS3-T03-F05 — Pattern guards and narrowing

## Estado

`TERMINADA`. Los guards, su cobertura y el refinamiento flow-sensitive comparten ahora el mismo
estado de flujo productivo consumido por sema, HIR, MIR y LLVM.

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
- Las comparaciones sobre locales/parámetros publican hechos de no-null e intervalos enteros en
  sus aristas true/false; `&&`, `||`, `!` y los joins conservan sólo hechos demostrados.
- Los tests de caso variant publican el tag activo y el extractor generado consume ese hecho.
- Una asignación o un argumento `ref`/`out` invalida los hechos del almacenamiento afectado.
- HIR/MIR publican `null-check=elided-flow`, `check=elided-flow` y
  `variant-case=proven`; LLVM omite la guarda de cero únicamente si el divisor está probado.

## Archivos modificados

- `src/Parser/Parser.cpp`: parse, orden de evaluación, lowering y cobertura conservadora.
- `src/Frontend/ProjectMethodBodyModel.cpp`: decodificación de wildcard guardado y cierre correcto
  de dominios abiertos, tabla compartida de hechos, joins e invalidación.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: contrato semántico de hechos por
  ocurrencia y por almacenamiento.
- `src/Frontend/ProjectOopHirModel.cpp`: consumo productivo de hechos de no-null/no-cero/caso.
- `src/Frontend/ProjectOopHirModelDumper.cpp` y `ProjectOopMirModelDumper.cpp`: decisiones visibles.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: eliminación demostrada de la guarda de división por cero.
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

## Validación dirigida

- `nsc_ns3_t03_pattern_guard_tests`: 14/14 con O0 y NSLink local; ejecución de guards,
  narrowing, dumps HIR/MIR, eliminación LLVM e invalidación por asignación/ref.

No se ejecutó baseline, CTest general, matriz, O1/O2/O3, instalación ni deploy.
