# Tanda 08: llamada C con parametros restrict

Se prepara una llamada desde NativeSharp a una funcion C con dos punteros
restrict, uno de ellos con apuntado const. La prueba usa objetos distintos y
comprueba el resultado; no demuestra deteccion general de aliasing ni emision
de noalias. Queda pendiente ejecutarla con la validacion agrupada.
