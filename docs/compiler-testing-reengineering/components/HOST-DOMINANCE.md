# Host.Dominance — Árbol de dominadores y frontera de dominancia

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo las inferencias marcadas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 18 archivos, 1,512 líneas y 48,578 bytes.
- **Inventario:** 81 símbolos físicos, 32 contratos y 256 matrices de prueba diseñadas.
- **Validación realizada:** lectura estática completa, extracción mecánica, trazado y segunda auditoría.
- **No realizado:** build, ejecución, baseline, cobertura o modificación del producto.

## 2. Responsabilidad y frontera

Host.Dominance recibe un `CfgModule` ya construido y, para cada función CFG, produce:

1. un `DominatorTree` con exactamente los nodos alcanzables en reverse postorder;
2. la relación de dominador inmediato;
3. parent, children y depth del árbol;
4. intervalos preorder/postorder para consultas de dominancia en tiempo constante;
5. la `DominanceFrontier` de cada nodo, única y ordenada por RPO;
6. consultas de dominancia, cadena de dominadores y dominador común más cercano;
7. dump textual y verificación independiente.

No calcula reachability ni RPO: ambos provienen de Host.CFG. No inserta PHI ni modifica MIR; SSA consume el
resultado posterior.

## 3. Ownership, módulo y función

`DominanceArena` posee módulos, funciones, árboles, nodos de árbol y fronteras. Destruye en orden inverso de
dependencia: fronteras, nodos, árboles, funciones y módulos. Los punteros a CFG son no propietarios.

`DominanceModule` conserva el `CfgModule`, la secuencia de análisis y un índice por identidad de `CfgFunction`.
`addFunction` conserva entradas en el vector y sólo indexa valores no nulos con CFG no nulo. La verificación
rechaza estados parciales, duplicados o funciones ajenas.

`DominanceFunction` une una función CFG con un árbol y una frontera. Los setters permiten construcción por fases;
un análisis aceptable exige ambos objetos y relaciones de procedencia consistentes.

## 4. Modelo del árbol

### 4.1 `DominatorTreeNode`

Cada nodo conserva:

- `CfgNode` fuente;
- dominador inmediato o null para root;
- children en orden de materialización;
- índice RPO;
- números preorder/postorder;
- depth.

### 4.2 `DominatorTree`

El árbol conserva función CFG, secuencia de nodos, lookup por identidad CFG y root. `addNode` registra el valor en
la secuencia y sólo lo indexa si existe su CFG. `findNode` devuelve null ante identidad desconocida.

`dominates(a,b)` usa inclusión de intervalos:

```text
pre(a) <= pre(b) && post(a) >= post(b)
```

Si cualquiera no pertenece al árbol, devuelve false. `strictlyDominates` añade `a != b`.
`immediateDominator` traduce parent de árbol a CFG y devuelve null para root o nodo desconocido.

`dominators(node)` sigue parents hasta root, luego invierte, de modo que entrega `[entry ... node]`. Para nodo
desconocido entrega vector vacío. `nearestCommonDominator` iguala depths y asciende ambos parents hasta converger;
si falta una identidad devuelve null.

## 5. Construcción

### 5.1 Gate de entrada y pipeline

`DominanceBuilder::build` rechaza módulo nulo, crea el módulo de salida y procesa cada función CFG en orden. Una
función requiere entry y RPO no vacío. El pipeline es:

```text
computeImmediateDominators
→ materializeTree
→ numberTree
→ buildFrontier
```

El builder conserva únicamente el primer error, pero no limpia `error_` al comenzar `build`; su contrato vigente
es usar una instancia nueva por operación.

### 5.2 Dominadores inmediatos

Se implementa el algoritmo iterativo de Cooper-Harvey-Kennedy. Primero se indexa el RPO y se rechaza cualquier
nodo nulo o no alcanzable. Entry se domina a sí mismo en el mapa temporal. Hasta punto fijo, cada nodo posterior:

1. filtra predecessors nulos, inalcanzables o todavía no procesados;
2. toma el primer predecessor válido como candidato;
3. intersecta el candidato con cada predecessor restante;
4. actualiza el mapa cuando cambia el resultado.

Un nodo alcanzable sin predecessor procesado falla. `intersect` avanza el nodo cuyo índice RPO es mayor hacia su
idom hasta que ambos punteros coinciden; falta de índice o parent produce null.

### 5.3 Materialización

Se crea un nodo de árbol por entrada RPO, en el mismo orden, y se guarda su índice. Root debe resolver al entry,
carece de parent y tiene depth cero. Para cada nodo no root, el idom debe existir y ser distinto del propio nodo.
Se enlazan parent, child y depth `parent+1`.

### 5.4 Numeración iterativa

`numberTree` usa una stack de `TreeFrame`, no recursión. Al entrar asigna preorder; después visita children en su
orden almacenado; al salir asigna postorder. Un contador compartido produce dos números únicos por nodo y
garantiza `pre < post`. Árbol nulo o sin root no cambia nada.

## 6. Frontera de dominancia

`buildFrontier` recorre nodos CFG en RPO. Sólo procesa joins con al menos dos predecessors alcanzables. Para cada
predecessor, `runner` asciende por idom hasta el idom del join, agregando el join al set de frontera de cada runner.

Los sets eliminan duplicados. Después cada set se convierte en vector y se ordena por índice RPO mediante
inserción. Se registra una entrada —incluso vacía— para cada nodo RPO.

`DominanceFrontier::frontier` devuelve una referencia a vector vacío estático cuando el owner no está registrado.
`contains` hace búsqueda lineal en el vector ordenado.

## 7. Dump textual

El dump siempre comienza con `dominance.module`; un módulo nulo agrega `<null>`. Una función inválida produce
`function <invalid>`. Una válida publica:

- nombre de función;
- nodos alcanzables, totales y excluidos;
- root;
- árbol indentado;
- por bloque: idom, depth, intervalo, cadena de dominadores y frontera.

Los bloques usan `bb<ID>`; null o root ausente usa `<root>`. El árbol se imprime con stack explícita, insertando
children en orden inverso para observarlos en el orden original. Un árbol vacío usa `-`.

## 8. Verificación independiente

### 8.1 Módulo

`verify` limpia error. Exige CFG fuente y exactamente un análisis por función. Rechaza funciones nulas, duplicadas,
ajenas, índices inconsistentes o CFG sin análisis.

### 8.2 Oráculo independiente de dominadores

El verifier recompone de manera independiente la relación de dominador inmediato sobre RPO. Conserva un único
parent esperado por nodo, cruza cadenas mediante índices RPO y repite el barrido hasta punto fijo. Entry se usa
como raíz durante el cálculo y se compara contra `null` en la representación materializada. El oráculo ocupa
`O(V)` memoria en lugar de materializar un set potencialmente cuadrático por cada nodo.

### 8.3 Forma del árbol

El árbol debe contener exactamente los nodos RPO, con root igual a entry. Root no tiene idom y depth es cero.
Cada nodo exige identidad/index únicos, posición RPO exacta, intervalo válido y dos números de traversal globalmente
únicos. Ningún nodo CFG inalcanzable puede aparecer.

Cada idom debe coincidir con el oráculo independiente. Parent, depth y children se recomputan; el vector de children
debe contener exactamente los hijos esperados, sin null ni duplicados. Los intervalos de cada hijo quedan dentro
del parent y los intervalos de siblings no se solapan.

### 8.4 Consultas

En grafos de hasta 64 nodos, el verifier compara exhaustivamente `dominates`, `strictlyDominates`, cadenas y NCD
contra la ascendencia esperada. En grafos mayores conserva comprobaciones lineales de raíz, identidad, parent,
intervalos y una cadena extrema; evita convertir la validación de escala en una matriz cartesiana ajena al contrato.

### 8.5 Frontera

El oráculo recorre cada join desde sus predecessors alcanzables hasta su idom esperado y registra el join en la
frontera de cada nodo visitado. El recorrido es iterativo y está acotado por `V` para detectar cadenas inválidas.

`vectorMatchesSet` exige cardinalidad, pertenencia y unicidad; después se comprueba orden RPO estrictamente
creciente.

## 9. Invariantes observables

1. Existe exactamente un análisis por función CFG.
2. Tree y frontier pertenecen a la misma función y al mismo árbol.
3. El árbol contiene cada nodo alcanzable una vez y ningún inalcanzable.
4. El orden de nodos del árbol coincide exactamente con RPO.
5. Root es entry, sin parent y con depth cero.
6. Todo nodo no root tiene un idom único, parent y depth coherentes.
7. Children son la inversión exacta de la relación parent.
8. Pre/post son únicos y forman intervalos de ancestro.
9. Todas las consultas coinciden con la relación matemática de dominancia.
10. Cada frontera es única, completa y ordenada por RPO.
11. Numeración y dump del árbol usan stacks explícitas, no recursión.
12. El verifier conserva el primer error de cada llamada.

## 10. Complejidad y límites vigentes

- CHK itera hasta punto fijo; su costo depende de iteraciones, edges e intersecciones por cadenas de idom.
- `intersect` asciende por parents con índices RPO y no usa recursión.
- Numeración cuesta `O(V)` tiempo y `O(altura)` memoria explícita.
- Construcción de frontier asciende desde cada predecessor de join hasta el idom del join.
- Ordenar cada frontier usa inserción, cuadrático en el tamaño de esa frontera.
- Consultas de dominancia son `O(1)`; cadena y NCD son `O(altura)`.
- El verifier usa `O(V)` memoria para idoms y children, más el tamaño real de las fronteras.
- Las matrices exhaustivas de consultas se reservan para grafos de hasta 64 nodos; a escala se verifican las
  invariantes estructurales y consultas representativas en espacio acotado.

## 11. Estrategia de prueba futura

Los 32 contratos se diseñan en `test-plans/HOST-DOMINANCE-TEST-PLAN.md`, con ocho matrices por contrato: 256
casos. Los fixtures futuros deben construir CFGs mínimos de línea, diamante, loop, múltiples joins, self-edge,
componentes inalcanzables y grafos profundos. El oracle principal debe ser una implementación sencilla de sets,
independiente del builder, además del propio verifier. El dump se compara byte a byte.

Los casos `NIGHTLY` ejercitan profundidad y amplitud suficientes para demostrar que numeración y dump no dependen
del stack de llamadas. La unidad contractual no es cada `if`, sino relaciones estables de dominancia.

## 12. Reconstructibilidad

El inventario, esta especificación, los 81 símbolos, 32 contratos y 256 casos permiten reconstruir ownership,
CHK, intersección, árbol, numeración, frontera, consultas, dump y verificación. La segunda auditoría está en
`audits/TANDA-07-HOST-DOMINANCE-AUDIT.md`.

**Resultado:** `RECONSTRUIBLE` para la revisión congelada, sin afirmar ejecución ni cobertura.
