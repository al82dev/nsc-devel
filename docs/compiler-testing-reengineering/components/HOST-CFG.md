# Host.CFG — Grafo de control derivado de MIR

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo las inferencias marcadas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 20 archivos, 1,224 líneas y 34,006 bytes.
- **Inventario:** 77 símbolos físicos, 26 contratos y 208 matrices de prueba diseñadas.
- **Validación realizada:** lectura estática completa, extracción mecánica, trazado y segunda auditoría.
- **No realizado:** build, ejecución, baseline, cobertura o modificación del producto.

## 2. Responsabilidad y frontera

Host.CFG proyecta cada función MIR definida a un grafo dirigido. Un bloque MIR se convierte en un `CfgNode` y
cada terminador produce cero, una o dos `CfgEdge`. Después calcula reachability, reverse postorder y salidas, y
verifica que toda esa información siga correspondiendo exactamente al MIR fuente.

La frontera incluye:

1. `CfgArena`, propietaria de módulos, funciones, nodos y aristas.
2. `CfgModule`, `CfgFunction`, `CfgNode` y `CfgEdge`, que conservan modelo y lookup por identidad.
3. `CfgBuilder`, que deriva topología y metadatos desde MIR.
4. `CfgTraversal`, que calcula RPO mediante DFS iterativo.
5. `CfgVerifier`, que vuelve a derivar y contrasta todas las relaciones.
6. `CfgDumper`, que publica una vista textual determinista.

CFG no cambia instrucciones MIR ni crea control nuevo. Tampoco calcula dominadores, fronteras de dominancia o
SSA; sólo entrega la topología validada que consumen esos componentes.

## 3. Modelo, ownership e identidad

### 3.1 `CfgArena`

La arena reserva cada entidad y la registra por categoría. Destruye primero edges, luego nodes, functions y
modules, evitando que un propietario superior sobreviva a sus relaciones. Los punteros a MIR y las aristas
entre objetos CFG son no propietarios. La creación es amortizada `O(1)` y la destrucción total es `O(V + E)`.

### 3.2 Módulo y función

`CfgModule` conserva el `MirModule`, una secuencia de funciones y un índice hash por identidad de
`MirFunction`. `addFunction` siempre conserva el elemento en la secuencia y sólo indexa funciones no nulas con
fuente MIR no nula. `findFunction` devuelve el objeto indexado o null.

`CfgFunction` conserva el `MirFunction`, nodos y edges en orden, un índice bloque-a-nodo, RPO, exits y
reachable exits. El primer nodo es entry. `addNode` conserva incluso una entrada nula en el vector, pero sólo
indexa nodos con bloque. El verifier convierte esos estados intermedios en aceptación o diagnóstico.

### 3.3 Nodos, aristas y adjacency

`CfgEdge` es una tripleta inmutable de source, target y kind. `CfgEdgeKind` distingue `Jump`, `BranchTrue` y
`BranchFalse`; `cfgEdgeKindName` produce `jump`, `true`, `false` o `unknown`.

`CfgNode` conserva el bloque MIR, listas ordenadas de incoming y outgoing y un flag reachable. Sus proyecciones
`predecessors()` y `successors()` ignoran edges nulos y deduplican nodos por identidad, conservando la primera
aparición. Esa deduplicación usa búsqueda lineal: para grado `d`, la proyección puede costar `O(d²)`.

## 4. Construcción del grafo

### 4.1 Módulo y funciones

`CfgBuilder::build` rechaza módulo nulo, crea el `CfgModule` y visita las funciones MIR en orden. Sólo las
definiciones no nulas generan CFG; los prototipos se omiten. `buildFunction` crea un nodo por bloque MIR en el
mismo orden y exige que el resultado tenga entry.

### 4.2 Terminadores y aristas

`buildEdges` observa exclusivamente la última instrucción de cada bloque:

- `Jump` crea una arista `Jump` al primer target;
- `Branch` crea `BranchTrue` al primer target y `BranchFalse` al segundo;
- `Return` y `Unreachable` no crean aristas;
- un bloque vacío, terminador nulo u otro opcode produce error.

`addEdge` resuelve el target mediante `findNode`. Un target externo a la función se rechaza antes de publicar
la arista. Una arista aceptada se registra exactamente en la función, outgoing del source e incoming del target.

### 4.3 Primer error

`fail` sólo escribe cuando `error_` está vacío. Un error posterior no reemplaza el primero. `build` no limpia
el error al comenzar; por tanto, el contrato vigente es una instancia nueva por construcción.

## 5. Traversal, reachability y exits

`CfgTraversal::reversePostOrder` implementa DFS sin recursión. `DfsFrame` conserva node y siguiente edge; una
stack explícita visita cada nodo una vez, ignora edges o targets nulos, acumula postorder y finalmente invierte
el vector. Una entry nula produce vector vacío.

Con hash de visited, el recorrido cuesta `O(V + E)` en tiempo y `O(V)` en memoria. El orden depende únicamente
del orden almacenado de outgoing edges, por lo que es reproducible para un mismo grafo.

`computeTraversal` primero limpia todos los flags. Marca reachable sólo a los nodos presentes en RPO. Después
recorre los nodos en orden y clasifica como exit cada uno sin outgoing; si además es reachable, lo incluye en
reachable exits. Una función puede contener exits inalcanzables y conservarlos separadamente.

## 6. Dump textual

El dumper siempre comienza con `cfg.module`. Un módulo nulo agrega `<null>`. Para cada función válida publica:

- nombre del símbolo;
- counts de nodes, edges, reachable y unreachable;
- entry;
- RPO;
- exits y reachable exits;
- cada bloque, su nombre, reachability, predecessors únicos y outgoing edges.

Los bloques usan `bb<ID>`. Una función inválida, un nodo inválido, una referencia nula o un edge nulo tienen
marcadores explícitos. Los conjuntos vacíos usan `-`. El orden textual sigue las colecciones almacenadas; no
ordena ni reconstruye el grafo durante el dump.

## 7. Verificación

### 7.1 Módulo y funciones

`verify` limpia error y funciones vistas. Exige source MIR. Cuenta definiciones MIR, comprueba que cada una tenga
CFG y que ningún prototipo lo tenga. El count CFG debe coincidir exactamente. Cada CFG function debe referir una
definición MIR única.

Nodos y bloques tienen correspondencia uno-a-uno y el mismo orden; no se aceptan vectores vacíos, punteros de
nodo duplicados o una entry distinta del primer bloque MIR. Los edges globales tampoco pueden ser nulos o
duplicados.

### 7.2 Registros de nodo y arista

Cada node debe resolver por su bloque al mismo puntero. Cada incoming debe existir en el registro de la función,
apuntar al node como target y ser localmente único. Cada outgoing cumple la condición simétrica como source.

Cada edge exige endpoints pertenecientes al mismo universo de nodos, lookup coherente y exactamente una
aparición en outgoing del source e incoming del target. El helper `countPointer` endurece el contrato de
`al menos una` a `exactamente una`.

### 7.3 Correspondencia con terminadores

El verifier reproduce la matriz del builder:

| Terminador MIR | Outgoing CFG exigido |
|---|---|
| `Jump` | uno, kind `Jump`, target exacto |
| `Branch` | dos, uno `BranchTrue` y uno `BranchFalse`, targets exactos |
| `Return` | cero |
| `Unreachable` | cero |

Bloques vacíos, terminadores nulos, opcodes no de control, cardinalidad incorrecta, kinds repetidos o targets
distintos fallan con el primer diagnóstico.

### 7.4 Traversal y salidas

El verifier recalcula RPO con `CfgTraversal` y exige igualdad posicional mediante `sameNodes`. Después contrasta
cada flag reachable con pertenencia al resultado. Finalmente deriva exits y reachable exits recorriendo nodes
en su orden original y exige igualdad exacta con las colecciones almacenadas.

## 8. Invariantes observables

1. Sólo las definiciones MIR tienen exactamente una función CFG.
2. Cada bloque MIR de una definición tiene exactamente un nodo, en el mismo orden.
3. Entry CFG corresponde al entry MIR y al primer nodo.
4. Cada edge publicado aparece una vez en función, source y target.
5. Kind, cardinalidad y target de edges coinciden con el terminador MIR.
6. RPO contiene cada nodo alcanzable una vez y ningún nodo inalcanzable.
7. Reachable flags equivalen a pertenencia en RPO.
8. Exits son exactamente los nodos sin outgoing; reachable exits son su subconjunto alcanzable.
9. Primer error de builder o verifier no se reemplaza dentro de la operación.
10. El dump conserva orden y marcadores explícitos de estados inválidos.

## 9. Límites vigentes

- `CfgBuilder::build` no limpia el error de una instancia reutilizada.
- `CfgFunction::addNode` y `CfgModule::addFunction` conservan elementos nulos en el vector aunque no los indexen.
- `CfgNode` permite registros parciales o duplicados hasta que interviene el verifier.
- La proyección única de adjacency tiene costo cuadrático respecto del grado por usar vector más búsqueda lineal.
- RPO omite silenciosamente edges nulos; el verifier los rechaza por la ruta de integridad del grafo.
- La existencia de una salida reachable no se exige en esta capa; loops infinitos pueden no tenerla.

Estas observaciones describen la revisión congelada. No autorizan una modificación del producto.

## 10. Dependencias, reconstrucción y prueba futura

| Dirección | Componentes | Contrato |
|---|---|---|
| entrada | MIR y Sema.Symbol | módulos, definiciones, bloques, terminadores, targets y nombres |
| interna | STL | vectors, unordered maps/sets, stack explícita y streams |
| salida | Dominance, SSA, Passes y diagnóstico | topología validada, RPO, reachability, exits y dump |

Un reemplazo compatible debe reproducir modelo, índices por identidad, orden de colecciones, matriz de edges,
DFS iterativo, metadatos derivados, verifier recomputacional y formato textual. Las 26 familias `CF-CFG-*`
dividen esa superficie. Cada una posee ocho matrices en `HOST-CFG-TEST-PLAN.md`; siguen siendo diseño, no código.

### Gate de las veinte preguntas

1. **Por qué existe:** separa topología de control de las instrucciones MIR.
2. **Problema:** hace consultables reachability, RPO, predecessors, successors y exits.
3. **Entradas:** módulo MIR, arena y stream.
4. **Salidas:** CFG, verdict, error o dump.
5. **Contratos:** 26 familias trazadas.
6. **Estructuras:** arena, módulo, función, nodo, edge e índices hash.
7. **Algoritmos:** proyección de terminadores, DFS iterativo y recomputación del verifier.
8. **Invariantes:** sección 8.
9. **Errores:** first-error latch con diagnóstico específico por frontera.
10. **Estados:** graph parcial/completo, node reachable/unreachable y verdict success/error.
11. **Ownership:** arena propietaria; MIR y relaciones son no propietarios.
12. **Interacción:** tabla de esta sección.
13. **Formatos:** dump CFG textual determinista.
14. **Complejidad:** `O(V + E)` salvo proyecciones de adjacency documentadas.
15. **Alternativas:** DFS recursivo o cálculo lazy no están implementados.
16. **Elección vigente:** materialización explícita con metadatos verificados.
17. **Evolución:** no reconstruida en esta fase de revisión congelada.
18. **Defectos históricos:** no se infieren; los límites vigentes están en sección 9.
19. **Pruebas:** plan contractual nuevo; batería histórica permanece congelada.
20. **Reimplementación:** secciones 2–10 e inventarios contienen los datos necesarios.

**Dictamen documental:** `RECONSTRUIBLE`. No equivale a pruebas implementadas ni a cobertura medida.
