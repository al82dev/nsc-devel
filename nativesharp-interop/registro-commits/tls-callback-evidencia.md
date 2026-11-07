# Evidencia de callback C almacenado en TLS

Con NSC `ab1886395db79d6f`, la construcción incremental y la suite de
sesiones pasaron. El control C y NativeSharp asignaron e invocaron un callback
`int(int)` almacenado en TLS; otro hilo observó su propia posición inicial.
El fixture pasó de O0 a O3 con biblioteca estática mediante LLD y DLL mediante
NSLink sin modificaciones.

También pasaron los controles dirigidos de records, enums, arreglos y
calificadores. La evidencia no cubre todas las firmas, convenciones ni
callbacks TLS; las DLL mínimas y el gate integral de tanda 8 siguen abiertos.
