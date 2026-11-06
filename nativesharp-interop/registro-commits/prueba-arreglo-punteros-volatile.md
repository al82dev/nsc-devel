# Controles del arreglo C de punteros a volatile

El fixture amplía el caso mixto de records con un arreglo global C de dos
punteros a enteros volatile. NativeSharp reasigna un slot, escribe y lee el
objeto apuntado y contrasta el resultado con una función C. Dos entradas
negativas comprueban la pérdida del calificador y la dirección del slot.

Los controles están preparados; la ejecución y la comprobación de IR se
realizarán conjuntamente. No se afirma todavía que hayan pasado.

Se añadieron una matriz de punteros, lectura de `const volatile`, rechazo de
escritura a través de ese puntero y rechazo al pasarlo a un parámetro mutable
sin volatile. El control de descarte busca el diagnóstico preciso, no solo
la palabra volatile, que también puede aparecer en la ruta del archivo.
