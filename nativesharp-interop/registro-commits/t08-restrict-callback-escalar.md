# Tanda 08: restrict en parametros de callbacks escalares

La expansion de typedefs de callbacks escalares utiliza los tipos originales
de Clang para conservar restrict en cada puntero de resultado y parametros.
No implica noalias; falta comprobar la captura y la ejecucion agrupada.
