# Entrega 231 — Grafo de compilación determinista / Deterministic compilation graph

## Español

`ProjectLoader` ya ordenaba fuentes, `ProjectReference` y `LibraryReference`, y
`ProjectGraphLoader` recorría esas dependencias en orden. El hueco estaba al incorporar módulos
NSMETA: `ProjectGraph::addPrecompiledProjects` conservaba el orden incidental en que el Driver había
descubierto descriptores y peers.

La implementación ordena los descriptores únicos por módulo y ruta de metadata; ordena las listas
de metadata por propietario; ordena la visibilidad de peers; y finalmente ordena y deduplica las
aristas fusionadas de cada proyecto. La raíz continúa al final y el orden topológico de proyectos
fuente no cambia.

`ProjectGraphTests` construye dos grafos equivalentes con descriptores NSMETA en órdenes opuestos y
exige volcados y ordinales idénticos (`Alpha`, `Zulu`, raíz). El contrato nuevo `NSC-GRAPH-001`
establece que enumeración del filesystem, orden XML y descubrimiento de metadata no pueden alterar
el producto. La prueba queda para el baseline único posterior a 234.

## English

`ProjectLoader` already sorted sources, project references, and library references, and the graph
loader traversed those canonical lists. The remaining gap was NSMETA insertion:
`ProjectGraph::addPrecompiledProjects` retained the Driver's incidental descriptor and peer order.

Unique descriptors are now sorted by module and metadata path, owner dependency lists and peer
visibility are sorted, and merged edges are finally sorted and deduplicated. The source root remains
last and source-project topological order is unchanged.

`ProjectGraphTests` expands equivalent graphs from opposite descriptor orders and requires identical
dumps and module ordinals. Compiler contract clause `NSC-GRAPH-001` records the invariant. Its test
is queued for the single post-234 baseline.
