# 387 — Dependencias y caché de comptime / Compile-time dependency caching

Cada `#run` registra una dependencia `source-expression` con identidad de ubicación y digest
SHA-256 de un dominio versionado más los bytes exactos de la expresión. Expresiones distintas no
pueden compartir entrada accidentalmente y cambiar cualquier byte invalida el resultado. / Every
`#run` records a `source-expression` dependency with a location identity and a SHA-256 digest over
a versioned domain plus the expression's exact bytes. Distinct expressions cannot accidentally
share an entry, and changing any byte invalidates the result.

Un caché compartido y seguro entre hilos sirve expresiones repetidas entre unidades del mismo
proyecto. La entrada guarda valor y pasos consumidos: un hit conserva el costo lógico y nunca
permite evadir el presupuesto del sandbox. El resultado expone hits, misses y el manifiesto para
pruebas y futura persistencia incremental. / A shareable, thread-safe cache serves repeated
expressions across units in one project. Entries retain value and consumed steps, so a hit keeps its
logical cost and never bypasses sandbox budgets. Results expose hits, misses, and the dependency
manifest for tests and future incremental persistence.
