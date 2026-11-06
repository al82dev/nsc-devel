# Punteros C a tipo incompleto en biblioteca separada

El fixture prepara una biblioteca C cuyo struct `Opaque` solo se define en el
archivo de implementación. NativeSharp recibe, pasa y almacena punteros a
ese tipo; C verifica los valores. Un control negativo intenta crear el tipo
incompleto por valor. Pendiente de ejecución y revisión de diagnósticos.
