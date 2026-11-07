# Reproducción agrupada de arreglos TLS escalares

El control C pasó con arreglos TLS de `unsigned int`, `double`, `_Bool`,
`const int` y `volatile int`, además de los arreglos anteriores. NSC
`2748b66609004562` rechazó los cinco en la proyección TLS de O0. La variante
DLL queda pendiente hasta superar la estática. La prueba cubre lectura,
escritura y aislamiento, y no pretende cerrar todos los tipos o calificadores.
