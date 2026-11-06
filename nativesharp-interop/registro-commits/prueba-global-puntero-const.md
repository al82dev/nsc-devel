# Arreglo global C de punteros a datos const

El fixture prepara un arreglo C `const int *[2]`, compartido entre C y
NativeSharp. La prueba positiva lee ambos datos, cambia un elemento por otro
puntero a const y pide a C comprobar el cambio. Una prueba negativa intenta
escribir a través del puntero. La salida se conserva en A: y la matriz cubre
O0–O3. Aún no se ha ejecutado y no demuestra soporte.
