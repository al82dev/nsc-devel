# Tanda 8: validación de TLS mediante `--link-lib`

El fixture enlaza una biblioteca C con TLS únicamente por `--link-lib` y valida ejecutable y DLL de O0 a O3, incluido el aislamiento entre hilos y el directorio TLS del PE. Un consumidor NativeSharp usa la DLL O0. Conserva la elección explícita de NSLink por opción y entorno, prueba bitcode sin TLS y compara la identidad de caché de un caso COFF sin TLS con NSLink explícito. La prueba pasó en A:.
