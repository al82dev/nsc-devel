# T08-004: CRT de DLL intermedia y propagación de errores

El primer enlace de la DLL falló por _tls_index; llvm-nm identificó su
definición en libcmt. Se declaran las bibliotecas CRT estáticas de la línea
base. La ejecución también devolvió cero pese al error: se reemplazan las
ramas de error dentro del bucle por un único destino que imprime los registros
y sale con código uno. La corrección está pendiente de validación.
