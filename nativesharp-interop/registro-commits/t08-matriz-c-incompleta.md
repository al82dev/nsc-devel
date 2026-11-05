# Tanda 08: matriz C de longitud exterior incompleta

- Se amplía el proyecto de tipos incompletos con `typedef int IncompleteMatrix[][3]`: comprueba el tamaño del puntero al alias y exige un diagnóstico de límite desconocido al pedir el tamaño de la matriz misma.
- La prueba complementa el arreglo incompleto unidimensional; comprueba que la dimensión interior conocida no haga pasar inadvertida la dimensión exterior incompleta.
- Estado: preparada, pendiente de ejecutar junto con la validación agrupada de la tanda. La prueba no cubre todas las operaciones ni cierra T08-013.
