# Prueba de proyecto TLS con LLD

La matriz preparada construye el proyecto de TLS C con `nsc build` y LLD
explícito de O0 a O3, inspecciona el enlazador reportado en JSON y ejecuta el
resultado para comprobar aislamiento entre hilos. La prueba aún no se ha
ejecutado y no sustituye la validación agrupada de la tanda 08.
