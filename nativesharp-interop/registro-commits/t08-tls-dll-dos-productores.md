# TLS de DLL compartida por productores precompilados

Dos bibliotecas NativeSharp publican plantillas que usan la misma global TLS de
una DLL C. El consumidor comprueba que ambas observan el mismo valor en cada
hilo y que el segundo hilo conserva un estado independiente del principal.
Los encabezados del productor se retiran antes de compilar el consumidor.

`tls-dll-plantillas/validar-optimizaciones.cmd` construyó y ejecutó los casos de
uno y dos productores en O0–O3: ocho ejecuciones correctas con dos hilos. La DLL
se construyó con LLD y se utilizó el enlazador configurado para el consumidor.
La muestra cubre una global TLS escalar, no todos los tipos ni todas las DLL.
