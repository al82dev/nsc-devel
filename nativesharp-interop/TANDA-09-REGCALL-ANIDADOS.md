# T09-003: registros flotantes anidados bajo regcall

En Win64, Clang oficial 23.1.0 expande en tres argumentos `float` tanto un
registro formado por otro registro de dos flotantes y un tercer flotante como
un registro que contiene un arreglo fijo de tres flotantes. Sus retornos LLVM
conservan, respectivamente, las formas `{ { float, float }, float }` y
`{ [3 x float] }`. NSC valida las formas, desplazamientos y atributos del
plan de Clang y emite el tipo de retorno correspondiente, tanto para llamadas
directas como a través de punteros.

La prueba dirigida compara un productor y control escritos en C con llamadas
desde NativeSharp. Pasaron las ocho combinaciones de O0 a O3 y enlace estático
o DLL, incluidos valores de retorno y copia del argumento:
`pruebas-tanda-09-regcall-nested/validar.py`, evidencia en
`A:/temp/nsc-t09-regcall-nested-2op99c0t`. La prueba de planes verifica
también el rechazo de retornos y coerciones alterados. Pasaron las regresiones
de registros planos, cuatro pruebas de sondeo ABI y punteros, comprobaciones
de transporte e identidad y 16 consumidores C++ precompilados de registros
planos.

El consumidor precompilado inicialmente falló al reconstruir el alias de
puntero. La reconstrucción ahora recupera sus registros dependientes del
catálogo vigente, comprueba identidad y layout, los ordena según dependencias
y contrasta las formas reconstruidas con Clang. Pasaron los 16 consumidores
con y sin encabezado, estática y DLL, O0-O3, en
`A:/temp/nsc-t09-hfa34-precompilado-6iocixnf`. También pasaron las
regresiones precompiladas de registros planos `regcall`, de punteros
`regcall` y de registros distintos `sysv_abi`, así como los rechazos de
conversiones incompatibles y las pruebas de transporte e identidad.

**Pendiente:** las muestras C++ anidadas y otras firmas o layouts no están
acreditados. Este avance no cierra T09-003 ni la tanda 9.
