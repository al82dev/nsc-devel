# Prueba de cadena de DLL y biblioteca precompilada

El fixture prepara un ejecutable que consume una DLL NativeSharp, que a su vez
usa un productor estático NativeSharp precompilado conectado al TLS de una DLL
C. La compilación de la DLL y del ejecutable oculta la copia del header C y la
restaura aun ante un fallo. La matriz O0–O3 queda pendiente del resultado.
