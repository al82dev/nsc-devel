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

La repetición con el script que exige los diagnósticos específicos pasó con
el mismo build; el resultado no cierra T08-016 ni la tanda.

Se preparan tres controles adicionales: dirección de un slot, dirección de una
fila y paso de una fila a `int**`. Están pendientes de ejecución e inspección
de sus diagnósticos; un rechazo genérico no acredita por sí solo el contrato.

Con build `9f214f1dc786940c` los tres controles se rechazaron. Para las dos
direcciones aparece el diagnóstico específico de const, acompañado también por
un mensaje genérico de dirección de campo de bits; la fila como argumento se
rechaza por incompatibilidad de tipos. El script se ajusta para verificar los
mensajes pertinentes; esa versión aún está pendiente de repetición.

La repetición del fixture con esas expectativas pasó de O0 a O3 para los casos
positivos y rechazó los tres nuevos usos. Se mantiene documentado el mensaje
genérico adicional y no se afirma cobertura completa de escape.
