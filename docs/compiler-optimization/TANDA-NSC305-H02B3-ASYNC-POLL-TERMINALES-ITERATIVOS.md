# NSC 3.0.5 — H02B3: terminales de async poll iterativos

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la autorrecursión de `rewriteAsyncPollReturns`. |
| Algoritmo | Worklist DFS de slots propietarios `unique_ptr`, con hijos insertados en orden inverso. |
| Símbolo | `ProjectOopHirModel::rewriteAsyncPollReturns`. |
| Protegido | Orden de hijos, payload, tipos, ordinals de future, estados ready/error, cleanup y terminal implícita raíz. |
| Fuera de alcance | Numeración de yields, rewrite de generator, análisis transitivo de await y máquina general HIR. |
| Terminación | Contrafactual profundo de Return/Throw, regresiones async/HIR, construcción de `nsc` y runtime estático. |

## Rojo reproducido

La implementación anterior invocaba `rewriteAsyncPollReturns` una vez por cada nivel de hijos. El
foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-async-poll-return-depth`, con 32,768 bloques, terminó antes
del cambio con `-1073741571` (`0xC00000FD`, stack overflow).

## Implementación

- La raíz se mantiene en su `unique_ptr` original y la worklist contiene direcciones de slots
  propietarios; no se copian subárboles.
- Los hijos se insertan de derecha a izquierda para conservar el recorrido observable de la versión
  recursiva: procesamiento de izquierda a derecha.
- `Return`, `Throw` y `YieldValue` se reemplazan en sitio y no se recorren después del reemplazo,
  igual que en la implementación anterior.
- `Return` conserva payload, result field 2, state `-1`, cleanup regions y continuation kind.
- `Throw` conserva payload, error field 4, state `-2`, cleanup regions y continuation kind.
- `YieldValue` conserva estado de reanudación, payload y cleanup.
- La detección y síntesis del retorno implícito se ejecuta una sola vez, después del recorrido, y
  sólo cuando `topLevel` es verdadero.
- Un slot nulo sigue abortando la reescritura con resultado nulo; no se alteró el contrato de error.

No se modificaron sintaxis, ABI, NSMETA, layouts de future/generator, diagnósticos ni formatos.

## Evidencia focal

- Foco `hir-async-poll-return-depth`: rojo previo `0xC00000FD`; después `PASS 5/5`.
- Profundidad probada: 32,768 bloques tanto para `Return` como para `Throw`.
- Se verificaron payload, field ordinal 4, state `-2`, cleanup, continuation y retorno implícito raíz.
- `nsc_ns3_t10_async_function_tests`: `PASS 9/9`.
- `nsc_ns3_t10_async_generator_tests`: `PASS 9/9`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato estático de runtime de `nsc.exe`: aprobado.
- La búsqueda estática deja únicamente la definición y dos llamadas ordinarias; no existe llamada de
  `rewriteAsyncPollReturns` a sí misma.
- `git diff --check`: limpio.

H02 permanece abierto: el siguiente alcance es el análisis de suspensión async todavía recursivo.
No se ejecutó baseline, deploy, instalación ni batería completa.
