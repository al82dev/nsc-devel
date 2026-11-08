# T08-003: objetos C generados con ThinLTO

Los objetos C inline se emiten después de ejecutar los backends ThinLTO. Una
compilación con sesiones Clang no internaliza símbolos porque todavía no se
conocen todas las referencias de esos objetos nativos. El bundle permite
agregarlos solo cuando no hubo internalización.

Una biblioteca que contiene objetos generados no publica sidecar ThinLTO: el
formato actual solo contiene bitcode y reemplazar la biblioteca por ese sidecar
perdería los objetos. El consumidor mantiene el archivo nativo completo. Esto es
una ruta conservadora provisional; la optimización entre bibliotecas con esos
objetos requiere transporte explícito en el sidecar y sigue pendiente.

La construcción y la matriz dirigida se registrarán por separado. NSLink,
LLVM/Clang y la instalación de C: no se modifican.
