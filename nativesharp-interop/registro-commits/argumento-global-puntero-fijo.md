# Escape del arreglo C de punteros constantes

Una función que recibe `int**` puede reasignar los slots; pasarle un arreglo
`int * const[2]` debe rechazarse. Se añade el caso negativo al fixture.
Pendiente de ejecutar y verificar el diagnóstico concreto.
