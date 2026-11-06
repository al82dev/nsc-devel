# Proyección de puntero C a arreglo fijo escalar

La función `c_matrix_ptr(const CMatrix*)` no se importaba porque
`mapFunctionType` carecía de proyección del arreglo apuntado. Se prepara el
mapeo de dimensiones fijas para escalares C y se conserva el calificador del
apuntado en la función. Se mantienen rechazados volatile y elementos anidados
que aún no tienen representación comprobada. Falta validación de extremo a
extremo y controles negativos de calificadores y dimensiones.
