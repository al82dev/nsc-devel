# T09-003: fábricas sobrecargadas con distinta aridad

Una plantilla precompilada perdía el método del objeto devuelto si el conjunto de fábricas sobrecargadas incluía otra declaración con diferente cantidad de parámetros. La ejecución directa sí funcionaba; el fallo reproducido está en `A:/temp/nsc-t09-instance-object-size-_s94sry9/O0-aridad/resultado-con-encabezado.log`. El selector descartaba todo el conjunto al exigir que cada candidato tuviera la aridad de la llamada.

Esta revisión conservaba los propietarios verificables de alternativas no variádicas, incluso si su cantidad de parámetros difería. La selección no declara viable una alternativa ni decide cuál llamar: la resolución y la comprobación de argumentos siguen en el consumidor. Entonces se rechazaban los conjuntos con candidatos no proyectables o variádicos; una [revisión posterior](t09-fabrica-variadica.md) cubre una fábrica variádica verificable. No se cambió NSMeta.

La prueba utiliza `makeArity(int) -> Probe*` y `makeArity(int, int) -> Other*`, cuyos métodos devuelven valores distintos. Las dos llamadas se ejecutaron directamente frente a Clang y NativeSharp, y desde plantillas precompiladas con y sin encabezado, biblioteca estática y DLL, en O0–O3. Evidencia: `A:/temp/nsc-t09-instance-object-size-8szya6o3` y `A:/temp/nsc-t09-instance-object-size-b2uv0cta`. También pasaron la prueba de identidad con Clang real y cinco pruebas seleccionadas de CTest; los controles anteriores de argumentos inválidos y empate siguieron rechazándose.

Este resultado no acredita argumentos predeterminados ni funciones variádicas, candidatos sin identidad verificable, otros flujos de receptor o todas las combinaciones ABI. T09-003 y la tanda 9 continúan abiertos.
