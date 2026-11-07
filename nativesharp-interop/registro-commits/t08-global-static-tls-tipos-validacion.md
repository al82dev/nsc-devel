# T08-004: validacion de matriz de globales C internas TLS

En la revision `128a5c873`, la construccion incremental termino con codigo 0.
La referencia C y NativeSharp pasaron O0–O3 para unsigned char, short,
unsigned long long, float, double, bool, enum, atomic int, arreglo y puntero.
Se comprobo declaracion TLS y load/store i32 atomicos seq_cst en IR. Pasaron
las regresiones de TLS estatico int, tipos internos no TLS y ambiguedad.

El control se agrega al gate de tanda, todavia no ejecutado completo. No se
acredita aislamiento entre hilos individual por tipo ni la representacion IR
de bool. Los commits `15989f138` y `792005c8d` incluyeron archivos de dos
trabajos distintos por uso simultaneo del indice Git; se conserva el historial.
