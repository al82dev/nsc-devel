# Rechazo de errores del controlador Clang

La apertura de sesión también revisa los diagnósticos capturados del controlador.
Clang puede construir un AST después de emitir un error de argumentos, por lo
que comprobar únicamente el resultado y los diagnósticos del AST no basta.

La corrección se verificará junto con los demás casos de la tanda 04.
