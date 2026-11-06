# Controles para globales C volatile void*

Se prepararon un global y un arreglo de `volatile void*`, que se entregan a
una función C con parámetro calificado. Dos entradas negativas comprueban el
rechazo de conversión que descarta volatile y de desreferencia de void*.
Los controles aún no se han ejecutado.

La prueba de inventario verifica también que los dos tipos se proyecten con
el calificador y las dimensiones esperados. Está pendiente de ejecución.
