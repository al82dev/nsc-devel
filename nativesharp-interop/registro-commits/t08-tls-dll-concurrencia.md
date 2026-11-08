# Tanda 8: primeros accesos TLS desde dos hilos

El control crea dos hilos, espera una señal de disponibilidad de cada uno y comprueba que `api2.dll` aún no esté cargada. Una señal común permite que ambos entren en el callback NativeSharp, donde cada uno comprueba y modifica sus propias variables TLS de `api.dll` y `api2.dll`. El hilo principal verifica después sus valores iniciales. El PE se inspecciona para comprobar que `api2.dll` no es importada estáticamente.

La versión inicial de `validar.cmd` pasó en O0–O3, con cinco procesos nuevos por nivel. Esa primera coordinación demostraba que ambos hilos estaban esperando antes de la señal y que `api2.dll` aún no estaba cargada. La prueba se amplió posteriormente con un observador en el límite de llamada al cargador: ver [evidencia instrumentada](t08-tls-dll-concurrencia-instrumentada.md). No se ha ejecutado la batería completa de la tanda 8.
