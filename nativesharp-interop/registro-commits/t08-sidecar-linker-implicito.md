# Tanda 8: selección automática con sidecar parcial

Un proyecto cuyo sidecar establece `build.optimization` pero omite `build.linker` reproducía NSL1003 al consumir una biblioteca ThinLTO: la existencia del archivo impedía seleccionar LLD. El lector distingue ahora la presencia real de la clave `build.linker`, y solo esa selección explícita impide la detección automática. No se reescriben sidecars existentes y las opciones explícitas de línea de comandos y entorno conservan su prioridad.

Las pruebas dirigidas con y sin TLS en bitcode pasaron. También pasaron los controles existentes de COFF sin TLS y de selección explícita. Queda pendiente la validación agrupada; este cambio no resuelve los cuatro controles TLS de DLL que siguen abiertos.
