# Prueba de proyecto TLS con LLD

La matriz preparada construye el proyecto de TLS C con `nsc build` y LLD
explícito de O0 a O3, inspecciona el enlazador reportado en JSON y ejecuta el
resultado para comprobar aislamiento entre hilos. La prueba aún no se ha
ejecutado y no sustituye la validación agrupada de la tanda 08.

La primera ejecución con build `d99972f438c24199` no llegó al enlace: el
resultado JSON indicó `C/C++ project references are not yet connected to
NativeSharp compilation`. La barrera procede de `CompilationInputManifest::
calculateIdentity`, que evita usar una identidad incompleta mientras no se
rastrean headers transitivos. No se considera aprobada la prueba; la salida
del script terminó sin sus mensajes de éxito aunque la invocación externa
reportó código cero. Se investigará la ruta existente sin caché sin retirar
la protección de caché con dependencias incompletas.

Tras habilitar la ruta sin caché, la matriz pasó O0–O3 con build
`2c5d0967206b6e62` y reportó LLD en JSON. No cierra la tanda 08.
