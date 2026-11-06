# Aislamiento TLS dentro de DLL C

Se prepara un caso independiente que enlaza la DLL con lld, conserva su
directorio TLS y llama desde NativeSharp una función que comprueba aislamiento
entre dos hilos. No prueba importar directamente un global TLS ni soporte de
TLS estático de NSLink. Pendiente construir y ejecutar.
