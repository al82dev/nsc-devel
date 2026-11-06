# Controles de arreglo global C volatile _Bool

NativeSharp escribe true y false, vuelve a leerlos y una función C comprueba
ambos valores. El script exige cargas i8 acquire y escrituras i8 release; una
prueba interna comprueba la marca y rechaza modo no MS. Queda pendiente la
validación agrupada y las operaciones compuestas.
