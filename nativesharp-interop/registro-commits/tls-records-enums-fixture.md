# Reproducción de records y enums TLS C

El control C pasó con un struct TLS, su puntero y arreglo, un enum TLS y su
puntero, incluyendo aislamiento entre hilos. NSC `d797816e3889d614` rechazó
los cinco globales en la proyección de O0. La variante DLL espera el resultado
estático. El fixture conserva los controles previos de punteros y calificadores.
