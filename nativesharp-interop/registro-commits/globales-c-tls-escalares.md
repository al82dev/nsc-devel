# Globales C TLS escalares

Se prepara un control C y un consumidor NativeSharp para `unsigned int`,
`short`, `double`, `_Bool`, `const int` y `volatile int` TLS. Cada consumidor
debe comprobar valores iniciales, escritura y lectura cruzadas y aislamiento
entre hilos. La matriz O0–O3 y la reproducción del rechazo están pendientes.
No cubre punteros, arreglos ni TLS de DLL.
