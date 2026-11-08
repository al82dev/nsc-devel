# T08: global C y plantilla precompilada en O0–O3

El validador de `pruebas-tanda-08-global-template` admite como primer argumento el nivel de optimización; sin argumento conserva O0. Las ejecuciones dirigidas con argumentos 1, 2 y 3 terminaron con código 0. La ejecución O0 previa también terminó con código 0.

Cada ejecución comprueba la presencia de la global en el NSMETA y ejecuta el consumidor separado, que modifica la global mediante la plantilla y lee el resultado por medio de una función C. Los registros están en `A:/nsc-interop-build/tanda08-global-template-run.log` para O0 y `A:/nsc-interop-build/tanda08-global-template-O1.log`, `-O2.log`, `-O3.log` para los restantes. Esta evidencia no constituye el cierre de la tanda 8.
