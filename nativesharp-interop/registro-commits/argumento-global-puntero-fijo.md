# Escape del arreglo C de punteros constantes

Una función que recibe `int**` puede reasignar los slots; pasarle un arreglo
`int * const[2]` debe rechazarse. Se añade el caso negativo al fixture.
Pendiente de ejecutar y verificar el diagnóstico concreto.

Con `f3f7271196c4666b`, el caso se rechazó por ausencia de sobrecarga viable;
la dirección del slot también se rechazó por pérdida de const. Se exige cada
diagnóstico específico para no confundir un fallo ajeno con el rechazo buscado.
