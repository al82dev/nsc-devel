# T09-003: fábrica C++ variádica como receptor precompilado

Una plantilla precompilada perdía el método de instancia cuando su receptor provenía de `makeVar(int, ...) -> Probe*`. La función y el método funcionaban directamente con Clang y NativeSharp; el fallo precompilado está en `A:/temp/nsc-t09-instance-object-size-2k4hwx38/O0-variadica/resultado-con-encabezado.log`. El selector omitía la identidad por tratarse de una firma variádica.

El selector conserva el propietario cuando se verifican identidad, USR, símbolo y referencia, sin elegir una sobrecarga ni resolver el ABI. La resolución de argumentos y el plan de llamada siguen siendo responsabilidad del consumidor. Si un candidato no puede proyectarse, no se publica un conjunto incompleto. No se cambió NSMeta.

La fábrica leyó un argumento variable `int` y devolvió el objeto con el valor esperado; el consumidor ejecutó su método con un argumento de tamaño oculto. Pasaron los controles directos de Clang 23.1.0 y NativeSharp y los consumidores precompilados, con y sin encabezado, en O0–O3, con biblioteca estática (`A:/temp/nsc-t09-instance-object-size-43nb36nc`) y DLL (`A:/temp/nsc-t09-instance-object-size-hbw0c81l`). También pasaron la prueba de identidad con Clang real y cinco regresiones seleccionadas de CTest. Los controles existentes de llamada ambigua y argumento incompatible continuaron rechazándose.

La muestra no acredita otras convenciones, tipos o promociones variádicas en esta ruta, ni argumentos predeterminados, flujos de receptor distintos o candidatos no proyectables. T09-003 y la tanda 9 siguen abiertos.
