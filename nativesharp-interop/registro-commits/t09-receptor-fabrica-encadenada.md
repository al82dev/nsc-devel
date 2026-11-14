# T09-003: método sobre resultado directo de fábrica

La llamada `makeWith(value)->size0(...)` funcionaba directamente con Clang y NativeSharp, pero una plantilla precompilada no reconstruía el método, porque el selector solo reconocía receptores almacenados en locales. El fallo reproducido está en `A:/temp/nsc-t09-instance-object-size-xhxnnxxu/O0-cadena/resultado-con-encabezado.log`.

El selector reconoce ahora una llamada de método por flecha cuyo receptor es directamente una fábrica, siempre que sus propietarios y referencias estén verificados. Conserva todos los propietarios candidatos y no infiere tipos por el nombre del método. Los controles unitarios comprueban que un receptor sin procedencia o sin resolución no se seleccione, y conservan las pruebas de escape, sombra y referencias conflictivas. No cambia NSMeta.

La llamada se ejecutó directamente y desde plantilla precompilada, con y sin encabezado, en O0–O3, frente a biblioteca estática (`A:/temp/nsc-t09-instance-object-size-rpn9bpf6`) y DLL (`A:/temp/nsc-t09-instance-object-size-hp3dzgd3`). Un argumento que incrementa un contador se evaluó una sola vez. Pasaron la prueba de identidad con Clang real y cinco regresiones seleccionadas de CTest.

Esta evidencia cubre la fábrica y el método de la muestra; no acredita otros flujos de receptor, convenciones, tipos de retorno o candidatos desconocidos. T09-003 y la tanda 9 continúan abiertos.
