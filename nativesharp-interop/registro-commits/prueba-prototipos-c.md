# Funciones C con y sin prototipo

El fixture contrasta `f(void)` con `f()` en C11, retorno y parámetro `void*`, retorno `void`, promociones de entero y flotante y rechazo de argumentos a una función con prototipo vacío. Se prueba en Windows x64 O0–O3; la preparación no demuestra soporte.

Inicialmente NSC rechazó la llamada de dos argumentos a `old_style()`. Tras conservar el dato de prototipo y usar la ruta de argumentos promovidos para la declaración C cdecl sin prototipo, el fixture pasó O0–O3: `byte` y `float` llegaron a una función C que recibe `int` y `double`. La función con `(void)` rechazó un argumento extra. También pasaron `nsc.cpp-interop.session`, callbacks C y la matriz de metadata C. Clang advirtió que la declaración sin prototipo está obsoleta y que C23 trata `()` de otra manera. No se probaron definiciones antiguas con parámetros declarados ni otras convenciones.
