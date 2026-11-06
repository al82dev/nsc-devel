# Control de índice de puntero desde arreglo C

El fixture prepara lectura y escritura mediante `volatile_pointer_array[1][0]`,
contrasta el valor desde C y comprueba el store release en IR. Debe ejecutarse
con los demás cambios de la tanda; no se declara validado todavía.
