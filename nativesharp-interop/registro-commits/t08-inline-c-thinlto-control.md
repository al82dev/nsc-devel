# T08-003: funciones C inline con ThinLTO

Se prepara una variante del fixture existente con `-flto=thin` en la
compilación directa y en los dos productores precompilados y su consumidor.
Conserva las comprobaciones de estado estático independiente y utiliza una
carpeta de salida distinta para no alterar las pruebas existentes.

El script captura los mensajes de cada fase por nivel O0–O3. Todavía no se
afirma compatibilidad: el resultado y los posibles fallos se registrarán tras
ejecutarlo con el compilador de desarrollo.
