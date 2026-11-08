# Tanda 8: aislamiento entre hilos de enum atómico TLS

El hilo principal escribe `STATE_READY` desde NativeSharp. Un segundo hilo,
creado desde C, comprueba que su propia instancia empieza en `STATE_IDLE` y
espera mediante eventos Win32 mientras el principal vuelve a leer su instancia.
Luego el trabajador comprueba que la suya conserva su valor. Los dos hilos
están vivos al mismo tiempo; la prueba no mide simultaneidad de instrucciones.

La ejecución cruzada pasó en O0–O3. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-enum-hilos-26633-5399`. Una primera
ejecución no enlazó los eventos Win32; se añadió `kernel32.lib` al fixture.
El caso cubre un enum de 32 bits y no cierra la tanda 8.
