# Punteros C a tipo incompleto en biblioteca separada

El fixture prepara una biblioteca C cuyo struct `Opaque` solo se define en el
archivo de implementación. NativeSharp recibe, pasa y almacena punteros a
ese tipo; C verifica los valores. Un control negativo intenta crear el tipo
incompleto por valor. Pendiente de ejecución y revisión de diagnósticos.

El primer intento con build `9f214f1dc786940c` compiló la biblioteca C pero
rechazó el global `opaque_current` en O0 con `C/C++ global type is not yet
mapped`; no llegó al enlace ni al control negativo.

Con build `a9fc5875a860a22c`, el consumidor pasó O0–O3 usando el puntero
opaco en retornos, parámetros y un global, con verificación desde C. La prueba
existente de tipos incompletos y la regresión de arreglos con const pasaron.
El local por valor se rechazó por la limitación general de lifetime C/C++, no
por un diagnóstico específico de tipo incompleto; esto no demuestra el
diagnóstico solicitado en T08-019. Se ajusta el control a ese mensaje real y
queda pendiente repetir el script ajustado.

El script ajustado pasó en la repetición con el mismo build y comprobó el
mensaje exacto de la limitación de lifetime. La tanda sigue abierta.
