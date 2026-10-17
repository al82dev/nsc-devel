# NSC 3.0.5 — H02B4: escaneo transitivo de await iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión del escaneo AST y del grafo de miembros usado por `lowerAwaitExpression`. |
| Algoritmo | Dos worklists DFS: nodos AST y ordinals de miembros, con orden estable y conjunto `visited`. |
| Símbolos | `collectAwaitedMemberOrdinals` y el análisis local `canSuspend`. |
| Protegido | Orden `first`, `second`, `children`; corte en `AwaitExpression`; bindings, límites y ciclos. |
| Fuera de alcance | Cambiar el resultado semántico de `canSuspend`, state-machine async y lowering general. |
| Terminación | Contrafactual AST profundo, regresiones async/HIR, construcción de `nsc` y runtime estático. |

## Rojo reproducido

Se extrajo primero el escáner AST conservando su recursión y su comportamiento. El foco
`NSC_PROJECT_OOP_LOWERING_FOCUS=hir-async-await-scan-depth`, con un binding await detrás de 32,768
nodos, terminó con `-1073741571` (`0xC00000FD`, stack overflow).

## Implementación

- `collectAwaitedMemberOrdinals` usa una pila de punteros AST no propietarios.
- El push inverso conserva el orden original: `first`, después `second`, después `children` en orden.
- Un `AwaitExpression` sigue siendo terminal para ese escaneo: registra su binding válido y no baja
  por sus hijos.
- El grafo transitivo de miembros usa una pila de ordinals y el mismo conjunto `visited` que la
  lambda recursiva anterior; los callees se insertan en reversa para conservar DFS.
- Ordinals fuera de rango, miembros repetidos y miembros sin sintaxis se siguen ignorando.

La implementación previa no contenía ningún caso base que devolviera `true`; por ello el análisis
`canSuspend` siempre concluía `false`. H02B4 preserva literalmente ese resultado. Corregir o redefinir
esa semántica requiere un alcance funcional separado con contrato y pruebas propias.

No se modificaron sintaxis, ABI, NSMETA, diagnósticos, layout async ni formatos.

## Evidencia focal

- Foco `hir-async-await-scan-depth`: rojo previo `0xC00000FD`; después `PASS 1/1`.
- Profundidad probada: 32,768 niveles AST, con binding ordinal 17 recuperado al final.
- `nsc_ns3_t10_async_function_tests`: `PASS 9/9`.
- `nsc_ns3_t10_async_generator_tests`: `PASS 9/9`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato estático de runtime de `nsc.exe`: aprobado.
- La búsqueda estática no encuentra `self(self)`, `scanSelf` ni `canSuspend(canSuspend)` en el
  archivo async.
- `git diff --check`: limpio.

H02 permanece abierto hasta auditar y convertir las rutas recursivas restantes de HIR. No se
ejecutó baseline, deploy, instalación ni batería completa.
