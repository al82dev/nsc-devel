# Tanda 8: prueba de DLL directa con TLS

El validador emite una DLL NativeSharp que enlaza el productor C TLS, verifica su directorio PE y ejecuta lectura, escritura y aislamiento entre hilos de O0 a O3 desde Python y desde un consumidor NativeSharp precompilado. Comprueba además que NSLink elegido por opción o entorno sigue siendo respetado, observando el directorio TLS ausente sin presentarlo como ejecución correcta. El control pasó en A:; no prueba la caché de bundle de DLL ni los cuatro controles de DLL C originales.
