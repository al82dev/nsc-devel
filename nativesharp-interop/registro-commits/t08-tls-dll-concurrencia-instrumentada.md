# Tanda 8: entrada concurrente a la carga de DLL TLS

El fixture instala temporalmente en la tabla de importaciones del ejecutable un observador de prueba para `LoadLibraryExW`. Para la primera carga de `api2.dll`, el observador espera a que ambos hilos hayan entrado antes de permitirles llamar a la función real. El control exige dos entradas, verifica antes que la DLL no está cargada, restaura la tabla al terminar y comprueba los valores TLS de cada hilo y del hilo principal.

`tls-dll-concurrencia/validar.cmd` pasó en O0–O3, con cinco procesos nuevos por nivel. La instrumentación demuestra el solapamiento en el límite de llamada observado y fuerza que ambos hilos lleguen antes de iniciar la carga real; no afirma que la implementación interna del cargador de Windows ejecute ambas cargas simultáneamente. La validación completa de la tanda 8 sigue pendiente.
