# Reproducción de records y enum TLS volatile

El control C pasó con struct, arreglo de struct y enum TLS `volatile`, más
struct `const volatile`, junto con los casos existentes y otro hilo. NSC
`8fa4034ae7c994a3` rechazó los cuatro globales en la proyección TLS de O0.
La variante DLL permanece pendiente hasta superar el caso estático.
