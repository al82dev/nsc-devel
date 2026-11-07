# Ruta de Clang en el control precompilado TLS

La primera ejecución de la matriz pasó el TLS interno O0–O3 y se detuvo
porque el consumidor precompilado declara encabezados del CRT y no recibió
`--cpp-clang-root`. El lanzador ahora proporciona la misma ruta de Clang de A:
que los otros proyectos. El control no se elimina ni se reduce; la matriz
completa debe ejecutarse de nuevo.
