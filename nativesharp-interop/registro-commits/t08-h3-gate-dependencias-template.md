# T08: dependencias C de plantillas en la validación agrupada

El validador de la tanda invoca `pruebas-tanda-08-dependencias-template/validar.cmd`, que ejecuta los controles existentes de alias, función y global C consumidos mediante plantillas de una biblioteca precompilada. Cada control se ejecuta con O0, O1, O2 y O3; el primer error detiene este conjunto y se comunica al validador de la tanda.

La ejecución dirigida de este conjunto terminó con código 0: los doce casos aparecen en `A:/nsc-interop-build/tanda08-dependencias-template.log` con la optimización correspondiente para productor y consumidor. Esta ejecución es independiente de la batería completa y no cierra T08-002 ni la tanda 8.
