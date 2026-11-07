# T08-004: validacion de matriz de globales C estaticas

La construccion incremental termino correctamente. La referencia C y la
matriz NativeSharp pasaron en O0, O1, O2 y O3 para signed char, unsigned short,
unsigned int, long long, unsigned long long, float, double e int*. El puntero
se inicializo con la direccion de otra global interna; las funciones inline
observaron las modificaciones del consumidor. La regresion de identidad y
ambiguedad de globales estaticas tambien paso.

Se agrega la matriz al gate de la tanda, que no se ha ejecutado completo con
estos cambios. Los calificadores, TLS, agregados y otros tipos no quedan
acreditados. T08-004 y la tanda 8 permanecen abiertos.
