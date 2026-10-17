# NSC 3.0.5 — B09A: walker iterativo de funciones de namespace

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `5d45c971d`.
- **Objetivo exacto:** eliminar el `STATUS_STACK_OVERFLOW` de la fase `members` al recorrer un
  AST con 1,024 controles ya parseados.
- **Naturaleza:** refactor correctivo no funcional; sustituir exclusivamente la recursión mutua
  `walkScope`/`walkNode` dentro de `ProjectNamespaceFunctionModel::build` por una worklist.
- **Cadena causal autorizada:** fixture profundo -> `compileProjectMembers` -> productores previos
  terminan -> `ProjectNamespaceFunctionModel::build` -> `walkScope`/`walkNode` ->
  `0xC00000FD`.
- **Archivos autorizados:** `src/Frontend/ProjectNamespaceFunctionModel.cpp`,
  `tests/unit/NamespaceFunctionLookupTests.cpp`, este expediente y tracking externo.
- **Prueba focal autorizada:** `nsc_namespace_function_lookup_tests` en el build existente de A:.
- **Invariantes:** primera pasada de `using`, ambientes heredados por valor lógico, namespaces
  léxicos, DFS preorden, orden `first`/`second`/`third`/`children`, `visited`,
  `directCallCallees`, candidatos, bindings, diagnósticos, sintaxis, semántica, API, ABI, NSMETA,
  CLI y artefactos 3.0.4.
- **Protegido:** `expressionName` (B09B), `ProjectMemberModel` (B10), parser, method bodies, HIR,
  MIR, LLVM, driver, pruebas ajenas, consumidores y repositorios distintos de `nsc`.
- **Prohibido:** aumentar stack, limitar profundidad, usar guard como solución, cambiar el fixture
  para reducirlo, baseline, suite completa, deploy, instalación, push, pull o fetch.
- **Criterio de terminación:** el caso profundo pasa; las pruebas históricas del target pasan; el
  diferencial no contiene instrumentación temporal ni cambios fuera de la ruta causal; expediente
  y tracking coinciden con la evidencia.

## Evidencia roja y contrafactual

La reproducción de 1,024 controles completa parser, generics, names y
`ProjectMemberModel::build`. Checkpoints crash-surviving confirmaron el plan de inicialización y el
binding precompilado; el último evento fue `namespaceFunctions.build.before`. No se alcanzó
`namespaceFunctions.build.after` y el proceso terminó con `-1073741571` (`0xC00000FD`).

Convertir temporalmente `ProjectMemberModel::scanMethodBody` a pila explícita no cambió el fallo.
Ese cambio y todos los checkpoints temporales fueron retirados antes de esta tanda.

## Diseño

La worklist usa dos clases de tarea: `Scope` y `Node`. Los ambientes y nombres de namespace viven
en arenas indexadas para que las tareas no copien el mapa de aliases/imports por cada nodo y para
que una realocación no invalide referencias. `Scope` conserva sus dos pasadas: primero acumula
directivas `using`; después programa sus hijos en orden inverso para que la pila ejecute el orden
original. `Node` conserva el preorden y programa `first`, `second`, `third` y `children` con la
misma técnica.

## Estado

Terminado.

## Implementación

`ProjectNamespaceFunctionModel::build` reemplaza las dos lambdas recursivas por una worklist LIFO
con tareas `Scope` y `Node`:

- cada tarea conserva índices, no referencias susceptibles a realocación;
- los ambientes sólo se copian al entrar en un scope, igual que antes, y no por cada nodo;
- la primera pasada de directivas `using` termina antes de programar hijos;
- los hijos se apilan en orden inverso para ejecutar el DFS original;
- `CallExpression` se procesa antes de su callee, conservando `directCallCallees`;
- namespaces encontrados como hijos de scope o desde un nodo conservan sus rutas distintas;
- `visited`, bindings, candidatos y diagnósticos permanecen en los mismos puntos lógicos.

`expressionName` no cambió: su cadena recursiva distinta permanece registrada en B09B.

## Evidencia

- Rojo previo del target focal: `TEST_EXIT_CODE=-1073741571`.
- `nsc_namespace_function_lookup_tests`: `PASS: 11 checks`, exit `0`.
- La prueba nueva compila un cuerpo con 1,024 controles y exige que el modelo de funciones de
  namespace se publique sin errores.
- `nsc` focal construido; contrato de host runtime estático: PASS.
- El fixture original `DeepMixedR04BControl`, con `-O0 -j 1`, ahora termina `members`,
  `inheritance`, `interfaces`, `generic-constraints` y `method-bodies`; reporta
  `project method-body semantic check: ok` y exit `0`.
- No se ejecutó baseline, suite completa, deploy, instalación, push, pull ni fetch.

## Resultado causal

La misma entrada que terminaba al entrar en `members` con `0xC00000FD` ahora completa el productor
y todo el target focal. El diferencial no cambia la resolución ni la forma de los bindings; cambia
únicamente dónde vive el estado de continuación del recorrido.
