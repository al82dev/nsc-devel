# Reproducción de arreglos TLS de punteros y callbacks

El control C pasó con `int*[2]` y un arreglo de callbacks TLS, junto con los
contratos previos y aislamiento entre hilos. NSC `c73dc89cc3ed0f95` rechazó
ambos globales en la proyección TLS de O0. La variante DLL queda pendiente.
