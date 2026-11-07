# Reproducción de arreglos TLS volatile

El control C pasó con arreglos `volatile` de entero sin signo, entero de 64
bits, double y bool, incluyendo aislamiento de otro hilo. NSC
`8333b70d84b5df92` rechazó los cuatro globales en la proyección TLS de O0.
La variante DLL permanece pendiente de ejecución.
