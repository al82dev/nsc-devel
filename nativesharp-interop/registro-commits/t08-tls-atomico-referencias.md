# T08: referencias al almacenamiento TLS atómico

`IdentifierAnalyzer.cpp` rechaza formar una dirección del global C atómico
mientras NativeSharp no represente punteros con calificación atómica. El
fixture verifica por separado que pasar ese global con `ref`, `out` e `in`
produce el diagnóstico de puntero atómico. Un control positivo comprueba los
tres modos sobre un entero ordinario; un error sintáctico no satisface la
comprobación negativa. No se han implementado punteros atómicos.

Validación dirigida: `pruebas-tanda-08-tls-c/atomic/validar.cmd`, código 0;
salida `A:/nsc-interop-build/tanda08-atomic-referencias-final.log`. Incluye la
comparación de IR y las ejecuciones O0–O3 del fixture. T08 sigue abierta.
