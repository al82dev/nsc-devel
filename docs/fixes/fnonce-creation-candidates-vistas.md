# Candidatas de sitios de creación FnOnce

El modelo conserva candidatas para declaraciones cuyo inicializador fue
analizado como lambda FnOnce. Cada candidata identifica la declaración, el
inicializador y los ordinales de callable, scope, local y lambda envolvente.
Al fusionar shards se aplican las bases independientes de scope, local y
lambda. Las candidatas repetidas se conservan, siempre con estado Unknown.

La colección registra sitios analizados: no demuestra que el inicializador
se haya ejecutado o terminado, ni que el sitio sea alcanzable. En particular,
una declaración después de `return` puede analizarse en un probe y queda
Unknown igual que las demás. Tampoco se resuelve el ordinal de la lambda
inicializadora ni se comprueba todavía su relación semántica en el productor.
No hay consumidor de seguridad y la barrera FnOnce permanece activa.

El productor conserva hasta 4096 candidatas por shard y señala el agotamiento;
el merge propaga ese indicador de forma acumulativa. El límite no acota el
total de candidatas ni la memoria de todos los shards. AST y modelo deben
mantenerse vivos al consultar los punteros; cancelación y presupuesto global
siguen sin modelarse en esta colección.

La unidad dirigida del merger comprobó los sitios `inner` y `sibling` en dos
callables, el sitio inalcanzable `ghost`, estados Unknown y remapeo, con uno y
cuatro workers. Un control sintético del helper verificó truncamiento con
límite dos y propagación del indicador entre shards. La compilación incremental
coordinada pasó sin warnings (`A:\nsc-vistas-evidencia\fnonce-candidates-fixed-build.log`)
y la unidad pasó 119 checks, observados en consola sin log persistente. Estas
verificaciones se hicieron con otros borradores FnOnce presentes en el árbol;
no prueban el commit selectivo de manera aislada, ni ejecución, cobertura
completa o solución del defecto interprocedural.

La primera versión de la prueba esperaba al menos seis candidatas y falló: la
inspección del modelo mostró exactamente cinco porque las dos lambdas `outer`
tienen trait Fn, mientras `inner`, `sibling` y `ghost` son FnOnce. La prueba
actual exige las identidades observadas; la evidencia original se conservó en
`A:\nsc-vistas-evidencia\fnonce-candidates-inspect-run.log`. Quedan pendientes
el contexto explícito de probes y errores, el flujo y orden de eventos, el
presupuesto global y las pruebas necesarias antes de usar estos datos para
diagnóstico o certificación. La interoperabilidad C/C++ queda fuera del alcance.
