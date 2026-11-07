# Evidencia de punteros TLS y calificadores

Con NSC `d797816e3889d614`, la construcción incremental y la suite de
sesiones pasaron. Los controles C y NativeSharp para `double*`, `const int*`,
`volatile int*`, `int* volatile` e `int* const` TLS pasaron O0–O3 en
biblioteca estática y DLL, con aislamiento entre hilos.

NSC rechazó asignaciones al slot `const` y al destino `const`; el IR estático
conserva acceso `volatile` al slot y escritura `volatile i32` al destino. Las
regresiones dirigidas de arreglos y global TLS también pasaron. Punteros a
otros tipos, DLL mínima y el gate integral de la tanda siguen pendientes.
