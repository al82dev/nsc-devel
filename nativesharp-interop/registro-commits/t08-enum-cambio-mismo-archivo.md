# T08: prueba de enum modificado en el mismo encabezado

El productor se compila con `Api.h` y el consumidor vuelve a usar esa misma
ruta después de cambiar `NHappy` de 4 a 5. El control exige que se rechace el
consumo y que el diagnóstico mencione `NMood`; restaura el archivo temporal.

Este control complementa las comparaciones unitarias del contenido y no afirma
por sí solo cuál condición del cotejo causó el rechazo. Validación pendiente.
