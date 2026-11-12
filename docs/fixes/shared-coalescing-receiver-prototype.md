# Publicación del receptor por coalescencia

En un payload de clase compartido, `Alias = this ?? Other` y `Alias = Other ??
this` publican el receptor cuando la rama que lo contiene se selecciona. La
matriz O0 encontró que ambas formas y una anidada pasaban semántica y emisión
para Rc y Arc, en constructor y destructor: 12 casos. Cuatro publicaciones
directas se rechazaron y 16 controles inocuos emitieron. Ningún consumidor se
ejecutó. Comandos, fuentes, diagnósticos y hashes estables:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-20261112/run-20261112T071142796620Z/results.json`.

El MIR de los casos izquierdo y derecho muestra `store` global con un
`null-coalescing` de dos hijos, uno `this` y otro `static-field-load`:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-mir-20261112/run-20261112T071224934713Z`.
El recorrido `origins` no descendía en ese nodo. Un control adicional puso
`this.IsReady()` solamente en la condición de un condicional que devuelve
valores ajenos antes de `??`: los cuatro casos inocuos emitieron, igual que
cuatro contrastes con `this` en el fallback. Evidencia:
`A:/nsc-fixes-agente-2-evidencia/receiver-coalescing-20261112/run-20261112T071305334264Z/results.json`.

La corrección recorre los dos operandos de `NullCoalescing` con dos hijos;
ambos pueden producir el resultado. La regresión exige 20 rechazos de
publicación y 20 emisiones inocuas, sin ejecutar consumidores. La compilación
incremental y la validación integrada pasaron los 40 casos; también pasaron
condicional, publicación directa y accesores binarios. Core ausente devolvió
77 y configurado inválido 1. Comandos, diagnósticos y hashes estables de
compilador, Core, metadata y runtime:
`A:/nsc-fixes-agente-2-evidencia/coalescing-integrated-validation-20261112/run-20261112T071714886780Z/results.json`.
El análisis es conservador ante un
fallback que resulte inalcanzable y no demuestra otras expresiones, cuerpos
binarios arbitrarios ni todos los caminos de publicación. El build contiene
otros cambios locales y el Core binario era preexistente.
`origins` también interviene al identificar retornos, argumentos y copias
locales; estos usos pueden volverse más conservadores, aunque las pruebas
indicadas solo acreditan sus casos medidos. `OptionalCoalescing`,
`NullCoalescingStore` y la procedencia de contenedores como destino quedan
fuera de esta corrección.
