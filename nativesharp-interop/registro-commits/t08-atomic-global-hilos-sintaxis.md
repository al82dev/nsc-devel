# T08: sintaxis del control concurrente

La primera ejecución de `validar-hilos.cmd` llegó al compilador y rechazó el
cuerpo del `for` sin llaves. Se añadieron las llaves exigidas por NativeSharp;
el control corregido aún no se ha vuelto a ejecutar y no acredita concurrencia.
