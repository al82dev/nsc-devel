# Matriz C global de punteros a datos const

Se prepara una matriz `const int *[2][2]`: la prueba mixta lee un elemento,
reasigna el slot y verifica el nuevo valor desde C. Dos controles intentan
escribir al dato const y convertir un elemento a `int*` mutable. Falta ejecutar
la prueba y examinar los diagnósticos antes de modificar el compilador.

Con build `9f214f1dc786940c`, el fixture positivo pasó O0–O3 y la regresión
de layout pasó. Los controles negativos se rechazaron respectivamente por
escritura no asignable y pérdida de const al inicializar `int*`; se ajusta el
script para exigir esos mensajes concretos. Falta repetir el script ajustado;
no se acreditan todas las dimensiones ni rutas de escape.
