# 388 — Reproducibilidad de comptime / Reproducible compile-time execution

La clasificación léxica del motor es ASCII explícito y no consulta el locale del proceso. La
evaluación no observa ruta actual, hora, zona horaria, ambiente, red, orden de directorios ni
aleatoriedad. El orden de directivas y dependencias es estrictamente el orden de bytes de la fuente.
/ The engine uses explicit ASCII lexical classification and never consults process locale.
Evaluation observes no current path, time, timezone, environment, network, directory order, or
randomness. Directive and dependency order is strictly source-byte order.

Cada resultado exitoso publica un digest SHA-256 canónico sobre un dominio versionado, la fuente
transformada y la secuencia de dependencias/valores. Estado del caché, scheduling y rutas quedan
fuera: ejecuciones frías, calientes y paralelas producen fuente y digest idénticos. Cambiar una
entrada cambia la identidad. / Every successful result publishes a canonical SHA-256 digest over a
versioned domain, transformed source, and dependency/value sequence. Cache state, scheduling, and
paths are excluded: cold, warm, and parallel executions produce identical source and digest.
Changing an input changes the identity.
