# Redeclaraciones C compatibles

El fixture prueba declaraciones sin prototipo seguidas de prototipos de un argumento y de cero argumentos. Los controles deben rechazar llamadas con aridad incompatible y aceptar llamadas válidas. La preparación no demuestra que pasen.

Inicialmente NSC aceptó `refined()` aunque una redeclaración posterior especificaba un argumento. Tras seleccionar un prototipo de la cadena de redeclaraciones de Clang, las llamadas válidas pasaron O0–O3 y se rechazaron tres llamadas de aridad incorrecta, incluidos ambos órdenes de declaración. También pasaron `nsc.cpp-interop.session` y los fixtures de callbacks. La prueba es C11 en Windows x64 y no cubre todas las combinaciones de declaraciones.
