# T08: comprobacion de record volatile con metadata

El consumidor sin encabezado ahora lee y escribe los miembros anidados de un record C declarado volatile. La biblioteca productora consulta ambos valores mediante funciones C para comprobar el intercambio entre lenguajes.

El validador compila y ejecuta el consumidor directo y la biblioteca con su consumidor solo metadata en O0-O3. Tambien exige que el IR del consumidor precompilado contenga lecturas y escrituras `volatile` de 8 y 32 bits; Clang aporta el control de lecturas. No pretende cubrir todos los layouts ni concurrencia.

`pruebas-tanda-08-volatile-campos-c/validar.cmd`: codigo 0, con la construccion incremental existente. La validacion completa de la tanda 8 sigue pendiente.
