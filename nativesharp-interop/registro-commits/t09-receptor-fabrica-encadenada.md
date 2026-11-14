# T09-003: método sobre resultado directo de fábrica

La llamada `makeWith(value)->size0(...)` funcionaba directamente con Clang y NativeSharp, pero una plantilla precompilada no reconstruía el método, porque el selector solo reconocía receptores almacenados en locales. El fallo reproducido está en `A:/temp/nsc-t09-instance-object-size-xhxnnxxu/O0-cadena/resultado-con-encabezado.log`.

El selector reconoce ahora una llamada de método por flecha cuyo receptor es directamente una fábrica, siempre que sus propietarios y referencias estén verificados. Conserva todos los propietarios candidatos y no infiere tipos por el nombre del método. Los controles unitarios comprueban que un receptor sin procedencia o sin resolución no se seleccione, y conservan las pruebas de escape, sombra y referencias conflictivas. No cambia NSMeta.

La llamada se ejecutó directamente y desde plantilla precompilada, con y sin encabezado, en O0–O3, frente a biblioteca estática (`A:/temp/nsc-t09-instance-object-size-rpn9bpf6`) y DLL (`A:/temp/nsc-t09-instance-object-size-hp3dzgd3`). Un argumento que incrementa un contador se evaluó una sola vez. Pasaron la prueba de identidad con Clang real y cinco regresiones seleccionadas de CTest.

Esta evidencia cubre la fábrica y el método de la muestra; no acredita otros flujos de receptor, convenciones, tipos de retorno o candidatos desconocidos. T09-003 y la tanda 9 continúan abiertos.

## Sobrecargas encadenadas

Una verificación posterior ejecutó una misma plantilla con `int` y `long`: las llamadas encadenadas seleccionaron `makeOver(int) -> Probe*` y `makeOver(long long) -> Other*`, respectivamente, y sus métodos devolvieron resultados distintos. El control C++ de Clang y NativeSharp directo pasó. Una llamada encadenada a `makeTie` con dos candidatos igualmente viables fue rechazada por ambigüedad, tanto con encabezado como sin él; el control Clang también la rechaza.

Las pruebas completas pasaron en O0–O3, con biblioteca estática (`A:/temp/nsc-t09-instance-object-size-rld9xfdo`) y DLL (`A:/temp/nsc-t09-instance-object-size-53c0_f2i`), incluyendo controles previos. Esto acredita esas firmas concretas; no cierra T09-003 ni certifica todas las sobrecargas.
