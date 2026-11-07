# Evidencia de escalares TLS volatile

Con NSC `8333b70d84b5df92`, la construcción incremental y la suite de
sesiones pasaron. Los controles C y NativeSharp para `volatile unsigned int`,
`volatile long long`, `volatile double`, `volatile _Bool` y
`const volatile int` TLS pasaron O0–O3 en estático y en DLL con CRT estático.
El fixture estático comprueba aislamiento de hilos, accesos `volatile` por
símbolo en IR y rechazo de escritura `const volatile`.

También pasaron las regresiones dirigidas de punteros y arreglos TLS. El
verificador de IR se corrigió para reconocer la sintaxis atómica `acquire` y
`release`; el resultado repetido fue correcto. Quedan otros contratos TLS,
las DLL mínimas y el gate integral de la tanda. No se modificó NSLink ni C:.
