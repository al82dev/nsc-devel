# Parte XII. Drozdek completo en NativeSharp

> **Edición NativeSharp 1.0.1-dev, Stage 67.** Este suplemento forma parte de la documentación
> versionada actual. Su inclusión no afirma un baseline 1.0.1 nuevo; los ejemplos conservan la
> evidencia de su edición de origen hasta la siguiente certificación explícita.

Esta parte convierte el temario de *Data Structures and Algorithms in C++*, cuarta edición, en una
ruta de estudio NativeSharp. No copia la prosa del libro: reconstruye sus definiciones, algoritmos,
casos de estudio y listados programables con el compilador NativeSharp actual. El índice de la
edición impresa se usa como lista de requisitos, no como sugerencia.

La cobertura tiene cuatro niveles verificables:

1. Cada capítulo, sección y subsección numerada de Drozdek aparece en los capítulos 48 a 63.
2. Cada listado de programa identificado en las figuras tiene un equivalente NativeSharp completo.
3. Las figuras no programables se traducen a trazas, invariantes, tablas o laboratorios reproducibles.
4. Los ejercicios no se copian literalmente; se sustituyen por problemas originales que ejercitan
   la misma técnica y que pueden resolverse sobre los proyectos entregados.

Los proyectos 3 a 27 forman el laboratorio ejecutable. El proyecto 25 concentra variantes
avanzadas; el 27 presenta las estructuras fundamentales como fuentes canónicas independientes.
Todos se publican con llaves Allman, cuatro espacios, sin espacio antes del paréntesis de una función
y con un máximo de 120 columnas en código.

## Mapa de las implementaciones fundamentales

| Estructura | Capítulo | Implementación completa | Operaciones ejercitadas |
|---|---:|---|---|
| Lista enlazada | 50.1 | `LinkedListExample.ns` | extremos, búsqueda, tres casos de borrado, limpieza |
| Pila | 51.1 | `StackExample.ns` | `Push`, `Top`, `Pop`, crecimiento LIFO |
| Cola | 51.2 | `QueueExample.ns` | buffer circular, wrap-around, crecimiento FIFO |
| Árbol binario | 53.1 | `BinaryTreeExample.ns` | cuatro recorridos, altura, hojas, espejo |
| BST | 53.3 | `BinarySearchTreeExample.ns` | inserción, búsqueda, min/max, borrado, validación |
| AVL | 53.4 | `AvlExample.ns` | LL, RR, LR, RL, inserción, borrado, validación |
| Grafo | 55.2 | `GraphExample.ns` | BFS, DFS, Dijkstra, Prim |
| Tabla hash | 57.2 | `HashTableExample.ns` | sondeo, tombstones, actualización, eliminación, rehash |

Los ocho listados aparecen dentro de esos capítulos y vuelven a reunirse como proyecto compilable
en el caso 47.27. Así el lector puede estudiarlos donde se explican o construirlos como una sola
suite, sin depender de referencias ocultas.

## 48. Programación orientada a objetos para estructuras de datos

### 48.1 Tipos abstractos de datos

Un tipo abstracto de datos se define por valores posibles, operaciones y leyes, no por sus campos.
Una pila promete `Push`, `Pop`, `Peek` y orden LIFO; decidir si usa array o lista es una decisión de
representación. En NativeSharp el contrato también debe indicar quién posee cada valor y qué pasa
cuando la estructura se destruye.

La especificación útil de una operación contiene precondición, poscondición, complejidad y efecto
sobre ownership. Para `Pop` de una pila de objetos no basta decir “devuelve la cima”: hay que fijar si
el resultado se mueve al llamador, se presta o se copia. El proyecto 6 muestra este contrato en una
pila usada por un evaluador real.

### 48.2 Encapsulación

Campos privados protegen la representación, pero la encapsulación sólo existe si ninguna operación
pública puede dejar la invariante a medias. Una lista con `head`, `tail` y `count` debe actualizar los
tres incluso cuando se elimina el único nodo. Una tabla hash debe reinsertar al cambiar de capacidad;
copiar buckets sin recalcular índices rompe búsquedas posteriores.

NativeSharp agrega una frontera que C++ antiguo y C# administrado tratan de forma diferente: el
destructor es parte de la corrección del ADT. Una clase dueña de nodos debe liberar todos los nodos,
romper ciclos cuando sea necesario y no destruir valores prestados.

### 48.3 Herencia y polimorfismo

La herencia sirve cuando existe sustitución semántica. El proyecto 2 usa una base abstracta de
empleado y políticas de pago concretas; el proyecto 4 separa algoritmo de ordenamiento y comparador.
Este segundo diseño permite comparar estrategias sin duplicar datasets ni instrumentación.

No se usa herencia para compartir tres líneas de código entre estructuras sin relación. Para una
cola y una pila, la coincidencia de almacenamiento no implica el mismo contrato. Composición y
genéricos conservan mejor las invariantes.

### 48.4 Punteros, referencias y arrays

Las referencias a clases NativeSharp representan identidad; los arrays son bloques contiguos con
longitud. Los nodos enlazados permiten crecimiento local, pero pagan asignaciones, indirecciones y
mala localidad de caché. Los arrays ofrecen recorrido contiguo y acceso O(1), pero crecer exige
reservar, mover y liberar.

Los equivalentes de punteros a función son delegados. Los equivalentes de referencias de salida son
`ref`, `out` e `in` en la superficie actual. El código histórico que negaba esas características no
se usa como autoridad porque el compilador ya llegó a stage 57.

Copiar una referencia no copia el objeto. Para valores owned, `move` transfiere la obligación de
destrucción. Esta diferencia debe revisarse en constructores de copia conceptuales, setters, arrays
de objetos y funciones que devuelven nodos.

### 48.5 Biblioteca genérica, iteradores y algoritmos

`List<T>`, `Stack<T>`, `Queue<T>`, `Dictionary<TKey,TValue>` y `HashSet<T>` cubren el papel de los
contenedores estándar. `foreach` usa el protocolo estructural de enumeración. Delegados y lambdas
ocupan el lugar de objetos función cuando se necesita inyectar comparación, hash o acción.

El objetivo pedagógico no es reemplazar toda estructura manual por una colección. Primero se
implementa la estructura para comprender su invariante; después se compara con la biblioteca para
separar aprendizaje algorítmico de código de producción.

### 48.6 Vectores y archivos de acceso por registro

El recorrido de arrays y `List<T>` incluye construcción, crecimiento, inserción, borrado, búsqueda,
ordenamiento y capacidad. Para un archivo de registros se fija un layout estable: identificador,
campos de ancho conocido, codificación y versión. `File.ReadAllBytes` y `File.WriteAllBytes` permiten
serializar sin depender del layout interno de una clase.

La biblioteca actual no expone seek posicional. El equivalente NativeSharp conserva registros de
tamaño fijo en un buffer, modifica el intervalo `offset = index * recordSize` y escribe el buffer.
No se finge que `WriteAllBytes` sea acceso aleatorio del sistema operativo: la diferencia de costo se
documenta y el formato permanece listo para una futura API `FileStream`.

### 48.7 Caso de estudio y laboratorio

El caso de archivo de acceso aleatorio se reconstruye como almacén de registros versionado. Las
pruebas mínimas son alta, consulta, actualización, baja lógica, reapertura, archivo truncado y versión
incompatible. El tutorial 5 cubre arrays y strings; los proyectos 1, 2 y 3 cubren ADT, polimorfismo y
algoritmos genéricos sobre objetos.

## 49. Análisis de complejidad

### 49.1 Complejidad computacional y asintótica

El tamaño de entrada debe definirse antes de contar. Para ordenar es el número de elementos; para un
grafo pueden ser `V` y `E`; para enteros de precisión arbitraria también interviene el número de bits.
Se cuentan operaciones dominantes y memoria adicional, separando costo del algoritmo de costo del
modelo de datos NativeSharp.

### 49.2 O grande, omega y theta

`O(g(n))` es cota superior asintótica, `Ω(g(n))` cota inferior y `Θ(g(n))` cota ajustada. Una
afirmación `f(n) = O(n²)` no dice que el algoritmo siempre tarde exactamente una constante por
`n²`; dice que, desde cierto tamaño, alguna constante multiplica a `n²` y domina a `f`.

Las propiedades usadas son transitividad, suma dominada por el término mayor, multiplicación de
costos anidados y eliminación de constantes. No se elimina un parámetro independiente: BFS con
listas cuesta `Θ(V + E)`, no simplemente `Θ(V)`.

### 49.3 Obtención de complejidad

Un loop simple suma su costo por iteración. Loops anidados independientes multiplican; loops cuyo
límite depende del exterior producen sumatorias. Dividir repetidamente por dos genera logaritmos.
Una recurrencia debe incluir trabajo local y tamaño de subproblemas.

El análisis de quicksort distingue partición lineal y distribución de pivotes. El de árboles usa la
altura `h`; sólo un árbol balanceado permite sustituir `h` por `log n`. El apéndice 61 desarrolla las
sumatorias armónica, `lg(n!)`, camino medio y mínimo de nodos AVL.

### 49.4 Mejor, promedio, peor caso y amortización

Búsqueda lineal tiene mejor caso O(1), peor O(n) y promedio dependiente de la distribución. El costo
amortizado de crecer un vector reúne muchas inserciones baratas y copias ocasionales: duplicar
capacidad mantiene O(1) amortizado por inserción aunque una inserción aislada cueste O(n).

El método agregado suma una secuencia completa; el contable asigna crédito; el potencial mide deuda
almacenada por el estado. El proyecto 7 permite observar capacidad circular sin confundir costo
amortizado con promedio probabilístico.

### 49.5 NP-completitud

P contiene problemas resolubles en tiempo polinomial; NP contiene problemas cuya solución propuesta
puede verificarse en tiempo polinomial. NP-hard describe al menos la dificultad de todo NP y
NP-complete agrega pertenencia a NP. Una reducción transforma instancias preservando sí/no en tiempo
polinomial; no es una comparación informal de que dos problemas “parecen difíciles”.

Los ejemplos de clique, 3-coloración, vertex cover y ciclo hamiltoniano reaparecen en el capítulo 55.
Para entradas pequeñas se usa búsqueda exhaustiva; para entradas grandes se documentan límites,
heurísticas o solvers especializados.

## 50. Listas enlazadas

### 50.1 Lista simple: inserción, eliminación y búsqueda

El proyecto 5 implementa nodo, cabeza, cola, cantidad, inserción frontal y final, búsqueda y borrado.
La eliminación se divide en cabeza, interior, cola y único nodo. El destructor recorre desde la
cabeza, captura `next` antes de destruir y termina con extremos nulos.

Las operaciones son O(1) en extremos mantenidos y O(n) para localizar por valor. Mantener cola no
abarata borrar la cola porque falta el predecesor; esa diferencia motiva la lista doble.

El siguiente laboratorio deja la implementación en el lugar donde se estudia. Incluye nodo,
contenedor, cabeza, cola, contador, inserciones, búsquedas, los casos de borrado y destrucción
determinista. El ejecutable que reúne los ocho módulos se reproduce completo en el caso 27.

<!-- GENERATED:DROZDEK_LINKED_LIST -->

### 50.2 Lista doble y circular

El proyecto 9 mantiene `previous` y `next` simétricos. El proyecto 10 cierra el último enlace sobre la
cabeza y resuelve Josephus. Una lista circular no puede limpiarse con “hasta null”; debe contar nodos,
detectar el regreso al centinela o romper primero el ciclo.

### 50.3 Skip lists

Una skip list superpone niveles enlazados. La búsqueda avanza mientras la siguiente clave no exceda
el objetivo y baja de nivel cuando ya no puede avanzar. Inserción conserva un vector `update` con el
predecesor de cada nivel. Con niveles aleatorios adecuados, búsqueda e inserción cuestan O(log n)
esperado y O(n) en el peor caso.

`DeterministicSkipList` del proyecto 25 usa el número de factores dos de la clave para obtener alturas
reproducibles. Esa política sirve para pruebas, no sustituye la distribución aleatoria de producción.

### 50.4 Listas autoorganizables

Move-to-front lleva el elemento consultado a la cabeza; transpose lo intercambia con su predecesor;
count ordena por frecuencia observada. No cambian el peor caso O(n), pero explotan localidad cuando
unas claves aparecen mucho más que otras. Deben medirse comparaciones y también costo de reorganizar.

El módulo `SequenceAlgorithms` implementa move-to-front sobre un array para hacer visible la política
sin mezclarla con asignación de nodos. El laboratorio pide repetir la secuencia de accesos de Drozdek
y comparar número total de comparaciones entre las tres estrategias.

### 50.5 Tablas dispersas

Una matriz dispersa guarda sólo valores no cero como triples `(row, column, value)`, listas por fila o
formato CSR. El producto punto disperso avanza dos listas ordenadas de índices y multiplica únicamente
coincidencias. Su costo depende de no ceros, no del área completa.

El proyecto 25 incluye el merge de índices para un producto fila-columna. La ampliación natural es
CSR por filas y CSC por columnas, suma lineal de filas y multiplicación con acumulador hash.

### 50.6 Colecciones estándar y caso de biblioteca

La colección genérica ofrece iteración y operaciones de propósito general; la implementación manual
expone la invariante. El caso de biblioteca se modela con listas separadas de catálogo, ejemplares,
usuarios y préstamos. Un préstamo enlaza identificadores, no hace dueño al usuario del ejemplar.

Pruebas: libro desconocido, ejemplar ya prestado, devolución doble, usuario con varias obras, baja de
un título con préstamos activos y destrucción del catálogo. Los proyectos 5, 9, 10 y 25 cubren todas
las representaciones de este capítulo.

## 51. Pilas, colas, colas de prioridad y deques

### 51.1 Pilas

Una pila basada en array conserva elementos en `[0, count)` y la cima en `count - 1`. Al crecer,
mueve en orden y libera el buffer anterior. Una pila enlazada inserta y quita en cabeza. Ambas ofrecen
O(1) por operación, amortizado para el array.

El proyecto 6 usa dos pilas para evaluar expresiones infijas con precedencia y paréntesis. Además de
`Push` y `Pop`, prueba entrada inválida, división por cero y limpieza durante una excepción.

Esta versión aislada muestra el almacenamiento redimensionable, el crecimiento amortizado, `Top`,
`Push`, `Pop` y la obligación de liberar el buffer.

<!-- GENERATED:DROZDEK_STACK -->

### 51.2 Colas y simulación bancaria

La cola circular del proyecto 7 usa `head`, `count` y módulo por capacidad. El tail lógico es
`(head + count) % capacity`. El caso bancario simula llegada, tiempo de servicio, espera, longitud
máxima y utilización. La distribución de llegadas es parte del modelo, no de la cola.

La implementación siguiente fuerza wrap-around y crecimiento. Al redimensionar copia en orden
lógico, no en el orden físico del buffer viejo.

<!-- GENERATED:DROZDEK_QUEUE -->

### 51.3 Colas de prioridad y heaps

Una cola de prioridad devuelve mínimo o máximo por prioridad. El heap binario del proyecto 13 ofrece
inserción y extracción O(log n), consulta O(1) y construcción bottom-up O(n). Empates requieren una
política: estabilidad mediante secuencia de llegada o libertad de orden.

### 51.4 Deques

El proyecto 11 implementa ambos extremos con array circular y lo aplica a palíndromos. Un deque
también sostiene BFS 0-1, ventanas monotónicas y work stealing. Deben probarse todas las combinaciones
de wrap-around y crecimiento, no sólo secuencias de un extremo.

### 51.5 Salida de un laberinto

El laberinto se representa como rejilla, cada celda almacena paredes o transitabilidad. DFS con pila
encuentra alguna salida; BFS con cola encuentra el menor número de pasos cuando cada movimiento vale
lo mismo. Guardar predecesor reconstruye la ruta sin copiar una ruta completa por estado.

El proyecto 16 contiene la infraestructura BFS/DFS y camino; sustituir ciudades por celdas produce el
listado de laberinto. El laboratorio exige entrada sin salida, inicio igual a salida, corredores
cíclicos y dos rutas de diferente longitud.

## 52. Recursión y backtracking

### 52.1 Definiciones y anatomía de llamada

Una definición recursiva necesita base y reducción. Cada llamada NativeSharp ocupa un frame con
parámetros, locales, retorno y limpiezas RAII. El costo de stack es O(profundidad), incluso si el
código ocupa pocas líneas.

Recursión de cola deja la llamada como última acción; recursión no terminal conserva trabajo. El
libro no presupone eliminación de tail calls: cuando la profundidad puede depender de datos hostiles,
la versión iterativa es el contrato seguro.

### 52.2 Recursión indirecta, anidada y excesiva

En recursión indirecta A llama B y B vuelve a A. La recursión anidada usa una llamada como argumento
de otra. Ambas requieren una medida de progreso demostrable. Fibonacci ingenuo ilustra recursión
excesiva; memoización cambia el árbol exponencial por una tabla lineal.

### 52.3 Backtracking y ocho reinas

Backtracking elige, marca, explora y desmarca. Ocho reinas conserva columnas y diagonales ocupadas;
una fila es el nivel de decisión. La prueba no sólo cuenta soluciones: valida que ninguna pareja
comparta columna ni diagonal.

El mismo esquema resuelve ciclo hamiltoniano y coloración exacta en el proyecto 25. RAII libera
buffers al salir, pero la restauración de marcas sigue siendo responsabilidad del algoritmo.

### 52.4 Descenso recursivo

Una gramática de expresiones puede separarse en `Expression`, `Term`, `Unary` y `Primary`. Cada
función consume tokens del nivel de precedencia correspondiente. El parser debe distinguir fin de
entrada, token inesperado, paréntesis faltante y división por cero en evaluación.

El proyecto 6 ofrece la variante iterativa con dos pilas. El ejercicio editorial pide implementar la
misma gramática por descenso recursivo y ejecutar un corpus compartido para demostrar equivalencia.

### 52.5 Curva de Koch

La curva sustituye un segmento por cuatro segmentos de un tercio. Tras profundidad `d` contiene
`4^d` segmentos y longitud `(4/3)^d` respecto del original. NativeSharp puede generar una secuencia
de instrucciones de dibujo o vértices aunque el libro no presuponga una biblioteca gráfica.

Se prueba número de segmentos, continuidad entre extremos y caja envolvente. Separar geometría de
render permite enviar después los vértices a una DLL gráfica mediante el ABI nativo.

## 53. Árboles binarios

### 53.1 Definiciones e implementación

Árbol, árbol binario y BST no son sinónimos. Un árbol binario limita hijos; un BST agrega orden. La
altura controla costo de búsqueda, inserción y borrado. El proyecto 8 implementa nodos, búsqueda
iterativa y recursiva, recorridos y destrucción postorder.

Primero se implementa un árbol binario general, sin imponer orden BST. El ejemplo construye una
forma explícita y ejecuta preorder, inorder, postorder, breadth-first, altura, hojas y espejo.

<!-- GENERATED:DROZDEK_BINARY_TREE -->

### 53.2 Recorridos

Breadth-first usa cola. Preorder, inorder y postorder pueden escribirse recursivos o con pila. Morris
inorder crea threads temporales desde el predecesor al nodo actual y los elimina al regresar; logra
O(1) memoria auxiliar sin dejar modificado el árbol. El proyecto 25 verifica el checksum inorder
después de restaurar todos los enlaces.

Un árbol threaded permanente distingue enlaces hijo de enlaces sucesor/predecesor mediante tags. La
inserción debe conservar ambos significados; seguir un thread como si fuera hijo produce ciclos.

### 53.3 Inserción y dos formas de eliminación

Inserción baja por comparación hasta un enlace vacío. Eliminación por copying reemplaza la clave por
predecesor o sucesor y elimina ese nodo. Eliminación por merging une subárboles, colocando el derecho
en el extremo apropiado del izquierdo. Ambas preservan inorder, pero cambian forma y patrones de
acceso.

El BST completo separa búsqueda, mínimo, máximo, inserción y borrado. El dataset elimina una hoja,
un nodo con hijos y un nodo con dos hijos; al terminar valida de nuevo el orden global.

<!-- GENERATED:DROZDEK_BST -->

### 53.4 DSW y AVL

Day-Stout-Warren convierte el árbol en vine mediante rotaciones derechas y después comprime con
rotaciones izquierdas para obtener altura logarítmica usando O(1) memoria. AVL repara localmente tras
cada operación y almacena altura o factor de balance. El proyecto 12 cubre las cuatro rotaciones y
borrado con rebalanceo.

Aquí está la implementación autocontenida: inserción, eliminación, alturas, factores de balance,
rotaciones LL, RR, LR y RL, búsqueda, recorrido y una validación estructural independiente.

<!-- GENERATED:DROZDEK_AVL -->

### 53.5 Árboles autoajustables y splay

Un splay tree mueve el nodo accedido a la raíz mediante zig, zig-zig y zig-zag. No garantiza altura
logarítmica en cada instante, pero sí costo amortizado O(log n). El proyecto 25 implementa splaying
recursivo y verifica que la clave consultada llegue a la raíz.

### 53.6 Heaps, treaps y k-d trees

El heap del proyecto 13 organiza un array como árbol completo. Un treap combina orden BST por clave y
orden heap por prioridad; prioridades aleatorias dan altura esperada logarítmica. El proyecto 25 usa
prioridades deterministas para reproducibilidad.

Un k-d tree alterna ejes por profundidad. La consulta rectangular poda el subárbol que no puede
intersectar el rango. El laboratorio usa puntos 2D y valida el resultado contra un recorrido lineal,
evitando confundir corrección con la eficacia de la poda.

### 53.7 Notación polaca y árboles de expresión

Prefija visita operador antes de operandos; postfija después; infija necesita precedencia o
paréntesis. Un árbol de expresión evalúa recursivamente, imprime en las tres notaciones y puede
aplicar simplificaciones como plegado de constantes. El proyecto 6 aporta tokenización y evaluación;
el ejercicio lo eleva a AST con ownership explícito.

### 53.8 Frecuencias de palabras

Tokenizar, normalizar y contar separa política lingüística de almacenamiento. Un BST produce salida
ordenada, una tabla hash favorece actualización, y un trie comparte prefijos. Los proyectos 8, 14 y
15 permiten implementar las tres versiones y comparar tiempo, memoria y orden de reporte.

## 54. Árboles multiway

### 54.1 Familia B-tree

Un B-tree de grado mínimo `t` guarda entre `t - 1` y `2t - 1` claves por nodo no raíz. Todas las
hojas tienen la misma profundidad. Inserción divide un hijo lleno antes de descender; borrado presta
de un hermano o fusiona para no entrar en un hijo mínimo.

`SmallBTree` del proyecto 25 implementa un 2-3-4 tree, el caso `t = 2`, con split top-down, búsqueda y
destrucción recursiva. El dataset fuerza splits de raíz e internos.

### 54.2 B-star, B-plus y prefix B-plus

B-star redistribuye con hermanos antes de dividir y busca mayor ocupación. B-plus conserva registros
en hojas enlazadas y usa claves internas como separadores; esto favorece rangos. Prefix B-plus acorta
separadores internos al prefijo mínimo que distingue bloques.

La adaptación NativeSharp modela páginas como arrays de capacidad fija y números de página, no como
punteros persistentes. Un split devuelve separador y nueva página al padre. El laboratorio mide
ocupación, lecturas de página y escaneo de rango.

### 54.3 K-d B-trees, bit-trees y R-trees

K-d B-tree combina partición multidimensional con páginas. Bit-tree decide por bits sucesivos de la
clave. R-tree agrupa rectángulos por cajas envolventes y permite solapamiento; una consulta debe
visitar todos los hijos cuyas cajas intersectan el objetivo.

Cada variante cambia la función de separación, no las obligaciones de una página: capacidad,
ocupación mínima, split, propagación, serialización estable y validación desde raíz.

### 54.4 2-4 trees, sets, multisets, maps y multimaps

El 2-4 tree es equivalente estructural al B-tree de grado mínimo dos. Set guarda claves únicas;
multiset agrega multiplicidad; map asocia una clave única a valor; multimap permite varias entradas.
En NativeSharp, `HashSet<T>` y `Dictionary<TKey,TValue>` cubren las variantes hash. Una estructura
ordenada multiway es necesaria cuando el orden o los rangos forman parte del contrato.

### 54.5 Tries y corrector ortográfico

El proyecto 14 implementa inserción, pertenencia y prefijos. El corrector carga diccionario, marca fin
de palabra y genera candidatos por eliminación, inserción, sustitución y transposición. Cada candidato
se valida en el trie; ranking puede incorporar frecuencia y distancia de edición.

Las pruebas incluyen palabra que es prefijo de otra, alfabeto fuera del rango admitido, duplicados,
diccionario vacío y UTF-8 multibyte. Un trie byte-oriented debe declararlo; no debe prometer caracteres
Unicode si indexa bytes.

## 55. Grafos

### 55.1 Representación y recorridos

Matriz de adyacencia usa Θ(V²); listas usan Θ(V + E). El proyecto 16 implementa BFS, DFS recursivo e
iterativo y reconstrucción de camino. En dirigido se distingue entrada de salida; en ponderado se
separa presencia de arista de peso cero.

### 55.2 Caminos mínimos

Dijkstra del proyecto 17 exige pesos no negativos. Bellman-Ford permite negativos y detecta ciclo
negativo alcanzable. Floyd-Warshall del proyecto 25 calcula todos contra todos mediante la invariante
“intermedios permitidos en 0..k”. El dataset clásico incluye pesos negativos sin ciclo negativo.

El laboratorio canónico usa una matriz ponderada y reúne BFS, DFS recursivo, Dijkstra y Prim. La
representación, las colas auxiliares, los conjuntos de visitados y las distancias aparecen en el
listado, no detrás de una biblioteca.

<!-- GENERATED:DROZDEK_GRAPH -->

### 55.3 Ciclos, union-find y árboles de expansión

DFS detecta back edges en dirigido; union-find detecta si una arista no dirigida une vértices ya
conectados. El proyecto 19 aplica path compression y union by rank en Kruskal. Prim mantiene la mejor
arista que cruza el corte. Ambos producen el mismo peso mínimo, no necesariamente las mismas aristas.

### 55.4 Conectividad

En no dirigido, BFS/DFS etiqueta componentes y puntos de articulación requieren tiempos de entrada y
low-link. En dirigido, componentes fuertemente conexas pueden obtenerse con Kosaraju o Tarjan. El
grafo condensado de SCC siempre es DAG y puede ordenarse topológicamente.

### 55.5 Orden topológico

Kahn, implementado en el proyecto 18, consume grados de entrada cero. DFS postorder ofrece una
alternativa. Procesar menos de `V` vértices demuestra ciclo. Varios órdenes pueden ser válidos; la
prueba debe validar cada dependencia, no una cadena literal arbitraria.

### 55.6 Redes: flujo máximo y costo mínimo

Edmonds-Karp usa BFS en la red residual y cuesta O(VE²). `MaximumFlow` del proyecto 25 mantiene flujo
directo y reverso y reproduce el valor 23 del dataset de seis vértices. El teorema max-flow/min-cut
permite certificar el resultado con un corte residual.

Para costo mínimo se agregan costos a aristas residuales y se buscan caminos de menor costo mientras
haya demanda. Potenciales permiten Dijkstra con costos reducidos; Bellman-Ford maneja costos
negativos iniciales. Capacidad y costo son propiedades independientes.

### 55.7 Matching

Matching bipartito máximo se reduce a flujo o usa caminos aumentantes. Gale-Shapley produce matching
estable respecto de preferencias, implementado en el proyecto 25. El problema de asignación ponderada
usa algoritmo húngaro. En grafos no bipartitos, blossoms representan ciclos impares contraídos.

El caso de representantes distintos es matching bipartito: conjuntos a la izquierda, candidatos a la
derecha y una arista por elegibilidad. Existe sistema de representantes exactamente cuando el matching
cubre todos los conjuntos; Hall proporciona la condición combinatoria.

### 55.8 Euler, Hamilton y coloración

Hierholzer construye circuito euleriano consumiendo cada arista una vez. Hamilton busca visitar cada
vértice y es NP-completo; el proyecto 25 usa backtracking con vértice inicial fijado. La diferencia es
esencial: Euler trata aristas y tiene algoritmo lineal; Hamilton trata vértices y no conoce algoritmo
polinomial general.

Coloración greedy depende del orden y entrega una cota, no siempre el cromático. Backtracking exacto
prueba `k` colores. Clique, vertex cover y coloración se conectan mediante complementos y reducciones,
pero cada transformación debe demostrarse.

## 56. Ordenamiento

### 56.1 Algoritmos elementales

Insertion sort desplaza un prefijo ordenado y es estable; selection sort minimiza swaps pero no es
estable por defecto; bubble sort intercambia inversiones adyacentes y puede cortar si no hubo cambios;
comb sort usa gaps decrecientes para eliminar “tortugas” lejanas. El proyecto 4 contiene los tres
primeros y el proyecto 25 agrega comb.

### 56.2 Árboles de decisión y límite inferior

Un algoritmo de comparación corresponde a un árbol cuyas hojas representan permutaciones. Distinguir
`n!` órdenes requiere altura al menos `lg(n!) = Ω(n log n)`. Counting y radix evitan esa cota porque
usan estructura de las claves, no sólo comparaciones.

### 56.3 Algoritmos eficientes

Shell sort compara subsecuencias por gaps; heap sort es Θ(n log n) in-place; quicksort tiene promedio
Θ(n log n), peor Θ(n²) y stack dependiente de partición; mergesort es estable Θ(n log n) con buffer.
El proyecto 4 instrumenta los cuatro.

Radix sort procesa dígitos con una pasada estable; counting sort cuenta claves de rango acotado. El
proyecto 25 implementa ambos. Deben documentarse signo, número de pasadas, base, estabilidad y costo
`Θ(n + k)` de memoria para el rango.

### 56.4 Algoritmos de biblioteca y polinomios

Una API de ordenamiento recibe comparación consistente. No debe prometer estabilidad si no la ofrece.
El proyecto 23 representa polinomios densos y suma por índices; una representación dispersa combina
listas ordenadas de términos igual que mergesort combina secuencias.

## 57. Hashing

### 57.1 Funciones hash

División usa módulo; folding suma grupos; mid-square extrae bits centrales del cuadrado; extraction
selecciona posiciones; radix transforma representación; universal elige una función de una familia
para limitar colisiones adversarias. `HashAlgorithms` implementa las tres primeras y deja las demás
como variaciones parametrizadas del mismo contrato.

### 57.2 Resolución de colisiones

Linear probing inspecciona posiciones consecutivas y sufre clustering primario. Quadratic probing
dispersa desplazamientos, pero necesita capacidad y parámetros compatibles. Double hashing usa un
segundo hash coprimo con la capacidad. Chaining, implementado en el proyecto 15, mantiene lista por
bucket. Bucket addressing guarda varias claves antes del overflow.

La eliminación en addressing abierto usa tombstone. Vaciar directamente una celda puede cortar la
cadena de búsqueda de una clave posterior. Rehashing elimina tombstones y cambia capacidad.

La tabla siguiente implementa direccionamiento abierto con sondeo lineal, estados vacío/ocupado/
tombstone, actualización, eliminación y rehash. El dataset provoca colisiones y reutiliza una tumba.

<!-- GENERATED:DROZDEK_HASH -->

### 57.3 Hash perfecto

Cichelli busca pesos de caracteres para nombres conocidos; FHCD construye un grafo de restricciones y
asigna valores a vértices. Son apropiados para conjuntos estáticos. La función generada debe validarse
sobre todo el conjunto y conservarse junto con su versión.

### 57.4 Cuckoo, hashing extensible y lineal

Cuckoo mantiene dos posiciones posibles y desplaza residentes; un ciclo obliga a rehash. El proyecto
25 implementa desplazamiento acotado. Extendible hashing usa directorio por prefijo y profundidades
global/local; linear hashing divide buckets gradualmente sin duplicar un directorio completo.

La implementación editorial calcula índices de directorio y modela split. Un sistema persistente debe
agregar páginas, overflow, recuperación ante escritura parcial y métricas de ocupación.

## 58. Compresión de datos

### 58.1 Condiciones y modelo

Compresión explota redundancia. Debe especificar alfabeto, modelo, representación de bits, encabezado,
padding y manejo de streams corruptos. Una lista de códigos impresa no es todavía un formato.

### 58.2 Huffman estático y adaptativo

El tutorial 23 construye frecuencias, heap, árbol y códigos prefijos. Para un símbolo único se asigna
al menos un bit. El decoder recorre el árbol por bits y rechaza caminos inexistentes o final dentro de
un código.

Huffman adaptativo actualiza árbol y pesos tras cada símbolo y permite comenzar sin tabla. Debe
mantener sibling property e intercambiar nodos sin romper padres. Su costo y sincronización son parte
del protocolo entre encoder y decoder.

### 58.3 Run-length y Ziv-Lempel

RLE reemplaza corridas por `(count, symbol)` y puede expandir datos sin corridas. LZ77 emite distancia,
longitud y siguiente símbolo usando ventana; LZW agrega frases a un diccionario sincronizado. El
proyecto 25 implementa conteo de tokens RLE, matching de ventana LZ77 y crecimiento de diccionario
LZW sobre los datasets clásicos.

### 58.4 Huffman con RLE

El pipeline combinado transforma primero una representación que produzca símbolos sesgados y después
aplica Huffman, o viceversa según dominio. El formato debe indicar orden de filtros. La prueba completa
es round-trip byte por byte, no sólo una razón de tamaños.

## 59. Administración de memoria

### 59.1 Ajuste secuencial

First-fit toma el primer hueco suficiente; next-fit continúa desde la última posición; best-fit toma
el menor suficiente; worst-fit el mayor. Fragmentación externa mide huecos inutilizables aunque haya
memoria total. El proyecto 25 implementa selección sobre una lista explícita de intervalos.

### 59.2 Ajuste no secuencial y buddy systems

Segregated lists agrupan tamaños. Binary buddy redondea a potencia de dos, divide bloques y fusiona
con el buddy calculado por XOR. Fibonacci buddy usa tamaños consecutivos de Fibonacci y reglas de
recombinación distintas. El módulo calcula el orden binario requerido y el laboratorio agrega listas
libres por orden.

### 59.3 Mark-and-sweep

Mark parte de raíces y visita referencias; sweep libera no marcados y limpia marcas. El proyecto 25
simula un heap con campos head/tail y marca siete celdas. Una implementación sin stack puede usar
pointer reversal como Schorr-Waite, restaurando cada enlace al finalizar.

### 59.4 Copia, incremental y generacional

Cheney copia alcanzables a semispace y usa forwarding pointers para preservar sharing y ciclos. Baker
intercala trabajo de colección con asignaciones. Yuasa usa barrera de escritura para que mutaciones no
oculten objetos blancos. Generacional explota que muchos objetos mueren jóvenes y necesita remembered
sets para referencias de viejo a joven.

`CopyReachable` del proyecto 25 muestra forwarding y asignación de destinos. El laboratorio completo
debe validar alias compartido, ciclo, raíz duplicada y objeto inalcanzable.

### 59.5 Colector in-place

El caso in-place marca sin espacio proporcional a la profundidad usando inversión temporal de
punteros y tags. La auditoría registra cada transición, demuestra restauración del grafo y después
construye la free list. Este algoritmo es educativo: NativeSharp productivo ya ofrece ownership y
RAII, pero comprender un collector explica los costos que un programador de C# normalmente no ve.

## 60. Búsqueda de cadenas

### 60.1 Matching exacto directo, KMP y Boyer-Moore

La búsqueda directa prueba cada alineación. KMP precomputa la longitud del prefijo propio que también
es sufijo y nunca retrocede en el texto. Boyer-Moore compara desde la derecha y combina bad-character
y good-suffix; Horspool conserva una tabla de desplazamiento más simple.

El proyecto 25 implementa directa, KMP y Horspool sobre bytes. Todos devuelven la misma posición en el
corpus y prueban patrón vacío, ausente, solapado y mayor que el texto.

### 60.2 Búsquedas múltiples y enfoque por bits

Aho-Corasick construye trie con failure links y encuentra múltiples patrones en O(texto + salidas).
Bitap representa estados en bits y actualiza con shifts y máscaras; el proyecto 25 incluye la versión
exacta para patrones de hasta treinta bits útiles.

### 60.3 Conjuntos de palabras y expresiones regulares

Un trie reconoce conjuntos finitos. Thompson convierte una expresión regular a NFA con transiciones
epsilon para concatenación, alternancia y estrella. Simular el cierre epsilon evita backtracking
exponencial. Capturas y lookaround no forman parte del núcleo y requieren semántica adicional.

### 60.4 Suffix tries, trees y arrays

Un suffix trie inserta todos los sufijos y puede costar Θ(n²) espacio. Un suffix tree compacta caminos;
Ukkonen construye online en tiempo lineal bajo supuestos de alfabeto. Un suffix array ordena índices de
sufijos y usa menos memoria; LCP acelera consultas repetidas.

El laboratorio valida cada estructura contra búsqueda directa y usa separadores distintos al combinar
dos textos. No se usan punteros dentro de datos persistentes: nodos se serializan por índice.

### 60.5 Matching aproximado

Levenshtein mide inserción, borrado y sustitución; el proyecto 22 reconstruye la tabla completa. Para
matching con hasta `k` errores se actualiza una frontera por posición o se usa Bitap aproximado cuando
el patrón cabe en una palabra. Una banda de ancho `2k + 1` evita trabajo que no puede mejorar el límite.

### 60.6 Longest common substring

Substring exige contigüidad, a diferencia de la subsecuencia del proyecto 21. El proyecto 25 incluye
DP que reinicia a cero al no coincidir. La alternativa suffix tree/array concatena textos con
separadores y busca el LCP máximo entre sufijos de orígenes distintos.

## 61. Apéndice Drozdek A: cálculo de O grande

### 61.1 Serie armónica

`H_n = 1 + 1/2 + ... + 1/n = Θ(log n)`. Agrupar términos entre potencias de dos ofrece cotas. Aparece
en análisis promedio, árboles aleatorios y algoritmos con costo inversamente proporcional al rango.

### 61.2 Aproximación de `lg(n!)`

`lg(n!) = Σ lg i = Θ(n log n)`. La integral y Stirling refinan la aproximación. Este resultado sostiene
el límite inferior de ordenamientos por comparación.

### 61.3 Quicksort promedio

Con pivote uniformemente distribuido, la recurrencia promedia todas las particiones y produce
Θ(n log n). El peor caso Θ(n²) sigue existiendo; aleatorizar entrada o pivote reduce la probabilidad,
no cambia la cota teórica del peor caso.

### 61.4 Camino medio de árbol binario aleatorio

La profundidad total esperada se relaciona con inserciones BST aleatorias y es Θ(n log n). Dividir por
`n` da profundidad media Θ(log n), aunque una permutación ordenada produce cadena.

### 61.5 Número mínimo de nodos AVL

`N(h) = 1 + N(h - 1) + N(h - 2)` con bases pequeñas. Crece como Fibonacci, por lo que altura es
O(log n). Esta recurrencia demuestra el límite, no sólo lo observa en un dibujo.

## 62. Apéndice Drozdek B: catálogo de algoritmos de biblioteca

La biblioteca se organiza por categorías, con su equivalente NativeSharp o implementación editorial:

| Categoría | Operaciones | Equivalente en esta edición |
|---|---|---|
| no modificadores | búsqueda, conteo, comparación | proyectos 3 y 25 |
| modificadores | copia, movimiento, reemplazo, eliminación | arrays, `List<T>` y ownership |
| partición y orden | partition, stable partition, sort | proyecto 4 |
| búsqueda ordenada | lower bound, upper bound, binary search | proyecto 3 |
| merge y set ordenado | merge, unión, intersección, diferencia | proyectos 4 y 15 |
| heap | make, push, pop, sort heap | proyecto 13 |
| permutaciones | next/previous permutation | laboratorio de backtracking |
| numéricos | accumulate, inner product, partial sums | proyectos 23 y 24 |

Un algoritmo de biblioteca no elimina su precondición. Binary search requiere orden; merge requiere
dos rangos ordenados; heap operations requieren la invariante heap. La API debe decidir si verifica,
documenta o expresa la precondición mediante tipo.

## 63. Apéndice Drozdek C: NP-completitud

### 63.1 Lenguajes, certificados y reducciones

Un problema de decisión se expresa como lenguaje de instancias sí. Un verificador polinomial recibe
instancia y certificado. Una reducción many-one calcula `f(x)` en tiempo polinomial y satisface
`x ∈ A` si y sólo si `f(x) ∈ B`.

### 63.2 Teorema de Cook

Cook-Levin demuestra que SAT es NP-completo al codificar el cómputo de una máquina no determinista en
una fórmula. Esta edición presenta la arquitectura de la prueba: tableau, restricciones de celda,
estado inicial, transición y aceptación. No confunde el teorema con una implementación práctica de SAT.

### 63.3 Laboratorio de problemas difíciles

Los solvers pequeños de Hamilton y coloración del proyecto 25 ofrecen verificadores independientes.
El ejercicio agrega clique y vertex cover sobre la misma matriz, prueba la relación
`S es clique en G` si y sólo si `S es independiente en el complemento`, y registra crecimiento al
aumentar `V`. El objetivo es observar explosión combinatoria con un contrato correcto, no presentar
backtracking como solución escalable.

## 64. Matriz de equivalencia de los listados de Drozdek

La siguiente matriz cubre los listados de programa señalados explícitamente por las figuras de la
cuarta edición. “Proyecto” identifica la implementación NativeSharp completa incluida en este libro.

| Figuras | Contenido programable | Proyecto NativeSharp |
|---|---|---:|
| 1.4 | operaciones de vector | tutoriales 5 y 15 |
| 1.5 | registros de archivo | 25, `CaseStudyAlgorithms.ns` |
| 3.2 | lista simple de enteros | 5 |
| 3.10 | lista doble | 9 |
| 3.17 | skip list | 25, `SkipListDemo.ns` |
| 3.25 | operaciones de lista estándar | tutorial 15 |
| 3.28 | sistema de biblioteca | 25, `CaseStudyAlgorithms.ns` |
| 4.4 | pila sobre vector | 6 |
| 4.9-4.10 | colas sobre array y lista | 7 y 22 |
| 4.13 | simulación bancaria | 7 |
| 4.18 | priority queue | 13 |
| 4.20 | deque | 11 |
| 4.24 | laberinto | 25, `CaseStudyAlgorithms.ns` |
| 5.6 | curva de Koch | 25, `CaseStudyAlgorithms.ns` |
| 5.13 | ocho reinas | 25, `CaseStudyAlgorithms.ns` |
| 5.20 | intérprete | 25, `ExpressionParser` |
| 6.8 | BST genérico | 8 |
| 6.10-6.11 | BFS y DFS de árbol | 8 y 16 |
| 6.15-6.17 | recorridos iterativos | 25, `TreeAlgorithms.ns` |
| 6.19-6.20 | árbol threaded y Morris | 25, `TreeAlgorithms.ns` |
| 6.23-6.24 | inserción normal y threaded | 8 y 25, `TreeAlgorithms.ns` |
| 6.29-6.32 | borrado por merging y copying | 8 y 25, `TreeAlgorithms.ns` |
| 6.56 | sift-down | 13 |
| 6.68 | frecuencias de palabras | 25, `CaseStudyAlgorithms.ns` |
| 7.43 | trie | 14 |
| 7.44 | corrector ortográfico | 14 y 25, `CaseStudyAlgorithms.ns` |
| 8.47 | representantes distintos | 25, flujo y matching |
| 9.8 | Shell sort | 4 |
| 9.11 | quicksort | 4 |
| 9.16 | radix sort | 25, `SequenceAlgorithms.ns` |
| 9.22 | suma de polinomios | 23 |
| 10.17 | hashing con buckets | 15 y 25 |
| 11.12 | Huffman combinado con RLE | tutorial 23 y 25 |
| 12.17 | colector in-place | 25, `MemoryAlgorithms.ns`, Schorr-Waite |
| 13.8 | longest common substring | 25, `StringAlgorithms.ns` |

## 65. Criterio de aceptación de la cobertura Drozdek

Esta parte sólo puede marcarse como terminada cuando se cumplen simultáneamente estas condiciones:

- todas las secciones 48 a 63 están presentes en el índice y el cuerpo;
- cada algoritmo declara precondición, invariante, complejidad y límites;
- cada listado de la matriz apunta a código NativeSharp completo o a una adaptación explícita;
- los proyectos compilan en `-O0` y `-O2` con el compilador registrado en el colofón;
- ambos ejecutables producen salida y código de retorno idénticos;
- el estilo editorial se aplica antes de recompilar las versiones publicadas;
- ninguna limitación de la biblioteca se disfraza como equivalencia inexistente.

La matriz es deliberadamente auditable. Si una futura edición de Drozdek o del compilador agrega un
tema, el hueco debe aparecer como requisito pendiente antes de que el libro vuelva a llamarse completo.
