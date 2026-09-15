# 391 — ThinLTO / Thin link-time optimization

`-flto=thin` conserva particiones LLVM paralelas, pero obliga particionado estable por identidad y
construye un índice global SHA-256 versionado a partir de los resúmenes de IR optimizado. El índice
no observa rutas temporales, scheduling ni estado del caché; las unidades continúan emitiéndose en
paralelo y pueden reutilizar objetos por identidad. / `-flto=thin` keeps parallel LLVM partitions,
forces stable identity partitioning, and builds a versioned global SHA-256 index from optimized-IR
summaries. The index excludes temporary paths, scheduling, and cache state; units remain parallel
and independently cacheable.

La prueba dedicada exige múltiples resúmenes, reproduce el mismo índice en dos compilaciones
paralelas y ejecuta ambos PE. / The dedicated test requires multiple summaries, reproduces the same
index in two parallel builds, and executes both PEs.
