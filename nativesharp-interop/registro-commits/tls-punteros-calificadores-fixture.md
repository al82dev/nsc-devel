# Reproducción de punteros TLS y calificadores

El control C pasó con `double*`, `const int*`, `volatile int*`,
`int* volatile` e `int* const` TLS, junto con el caso anterior y aislamiento
entre hilos. NSC `df824a793de82e55` rechazó esas declaraciones en la
proyección TLS antes de enlazar. La variante DLL queda pendiente de ejecución.
