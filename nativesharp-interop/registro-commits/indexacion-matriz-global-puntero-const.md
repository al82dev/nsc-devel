# Conservación de const al indexar matrices C

El resultado de un índice parcial de una matriz sigue siendo un arreglo. Se
prepara la propagación de la marca de apuntado const también a ese resultado,
de modo que la siguiente indexación la entregue al puntero final. La misma
protección de direcciones impide perder el calificador anidado. Pendientes la
construcción incremental, los controles negativos y la regresión de layout.
