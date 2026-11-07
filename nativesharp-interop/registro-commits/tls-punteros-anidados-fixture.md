# Reproducción de punteros TLS anidados y void

El control C pasó con `int**` y `void*` TLS, conversiones y aislamiento entre
hilos junto con las regresiones existentes. NSC `a83d41a567fa4257` rechazó
ambos globales en la proyección TLS de O0. La variante DLL permanece pendiente.
