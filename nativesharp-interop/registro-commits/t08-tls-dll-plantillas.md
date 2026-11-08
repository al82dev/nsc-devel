# TLS de DLL en una plantilla precompilada

Una biblioteca NativeSharp publica una plantilla que usa una global TLS de una
DLL C. El consumidor invoca la plantilla en el hilo principal y mediante un
callback en un segundo hilo; comprueba los valores iniciales y la separación
del estado por hilo, además de consultar la DLL directamente.

El productor y consumidor compilaron y ejecutaron en O0 tras retirar el
encabezado original del productor. La prueba utiliza los controles C existentes
de la DLL TLS, LLD para construirla y el enlazador configurado para el consumidor.
Se reproduce con `tls-dll-plantillas/validar.cmd`.

La muestra cubre una global escalar y dos hilos; no acredita las demás
optimizaciones, tipos ni múltiples productores independientes.
