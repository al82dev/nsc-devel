# Punteros C a tipo incompleto en biblioteca separada

El fixture prepara una biblioteca C cuyo struct `Opaque` solo se define en el
archivo de implementación. NativeSharp recibe, pasa y almacena punteros a
ese tipo; C verifica los valores. Un control negativo intenta crear el tipo
incompleto por valor. Pendiente de ejecución y revisión de diagnósticos.

El primer intento con build `9f214f1dc786940c` compiló la biblioteca C pero
rechazó el global `opaque_current` en O0 con `C/C++ global type is not yet
mapped`; no llegó al enlace ni al control negativo.
