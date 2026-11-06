# Prueba de coalescencia en global C volatile

El fixture prepara `??=` sobre un puntero C volatile inicialmente nulo y
luego no nulo. Comprueba el resultado en ejecución y examina las cargas y
escrituras de los bloques de fallback específicos en LLVM IR para impedir que
otras operaciones hagan pasar la comprobación. Su ejecución corresponde a la
validación agrupada de la tanda 08.
