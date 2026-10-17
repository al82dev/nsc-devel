# NSC 3.0.5 — N01B: `ProjectNameLookup::bindNode` iterativo

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `e0b252a6e`.
- **Entrada roja:** proyecto focal con 16,384 `MemberExpression`, ejecutado hasta
  `compileProjectNames`/`--check-project-members` en O0 y J1.
- **Evidencia causal:** N01A demuestra que `expressionQualifiedName` ya retorna; inmediatamente
  después, `bindNode` continúa llamándose por `node->first()` una vez por segmento. La función
  también se auto-invoca para `second`, `third`, `children` y expresiones `arrayCount`.
- **Hipótesis única:** sustituir la auto-recursión de `bindNode` por frames explícitos elimina el
  overflow sin cambiar el binding ni el orden de diagnósticos.
- **Archivo y símbolo autorizados:** `src/Frontend/ProjectNameLookup.cpp`, exclusivamente
  `ProjectNameLookup::bindNode`; prueba focal existente, este expediente y tracking.
- **Orden congelado:** trabajo del nodo; `arrayCount` de modificadores en orden; throws;
  `typeSyntaxes`; constraints; `first`; `second`; `third`; `children` en orden fuente.
- **Scopes congelados:** una copia de `LookupEnvironment` sólo al entrar en un tipo nombrado; el
  entorno heredado se comparte por índice en el resto; las declaraciones namespace continúan
  delegándose a `bindScope` sin cambiar su doble pasada de usings y miembros.
- **Contratos congelados:** candidatos, bindings, diagnósticos y orden, visibilidad, aliases,
  namespaces, API, ABI, NSMETA, sintaxis, semántica y artefactos.
- **Protegidos:** `bindScope`, `bindTypeSyntax`, resolvers, miembros, HIR, MIR, LLVM, driver y
  consumidores.
- **Prohibido:** generalizar `bindScope`, alterar pruebas existentes salvo elevar el gate profundo
  de `generics` a `names`, baseline, suite completa, deploy, instalación o red.
- **Gate:** rojo reproducido después de N01A; implementación sin auto-llamadas; test focal de
  16,384 segmentos completa `names`; `PASS 12/12`; build focal de `nsc`; diff auditado; tracking y
  commit local aislado.

## Implementación

`bindNode` usa ahora una pila LIFO de frames `{node, environment, stage}`. Los entornos heredados se
referencian sin copia; una `std::deque` conserva únicamente las copias que ya exigía entrar en un
tipo nombrado y mantiene punteros estables. `Stage::AfterArrayCounts` reanuda el nodo después de
procesar las expresiones de dimensiones, por lo que throws, listas de tipos, constraints y
descendientes conservan su posición exacta.

Los descendientes normales se apilan en orden inverso para observar el mismo preorden anterior:
`first`, `second`, `third` y después `children` en orden fuente. Una declaración namespace sigue
delegándose a `bindScope`; no se alteró su doble pasada ni su entorno de aliases/imports.

## Evidencia focal

- Rojo heredado de N01A: el helper calificado retorna, pero `names` cae con `0xC00000FD` mientras
  `bindNode` desciende por `first()`.
- Regresión elevada a `compileProjectNames` con 16,384 segmentos: `PASS`.
- Target completo `nsc_namespace_function_lookup_tests.exe`: `PASS: 12 checks` en 10.6 s y, tras
  el gate final, nuevamente `PASS: 12 checks`.
- Los casos existentes de imports, nombres calificados, aliases, namespace actual y AST de 1,024
  controles permanecen verdes en el mismo target.
- Build focal de `nsc.exe`: terminado; contrato de host runtime estático aprobado.
- Búsqueda estática: la única llamada restante a `bindNode` está en `bindScope`; `bindNode` ya no se
  auto-invoca.
- `git diff --check`: sin errores.
- Baseline, suite completa, deploy e instalación: no ejecutados.

## Estado

N01B terminado. N01 terminado.
