# Tanda 08: objetivos aislados de pruebas de sintaxis

La validacion se detuvo porque la configuracion ON desactiva la suite general y
por ello no crea los dos ejecutables de cache agregados al gate. Cuando estan
activadas las pruebas de sesion de interop y desactivada la suite general,
CMake define solo esos dos objetivos; con la suite completa evita duplicarlos.
Falta reconstruir y ejecutar la bateria agrupada.
