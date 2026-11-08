# TLS C en plantillas: validación agrupada por optimización

`validar-tls-optimizaciones.cmd` construyó los productores y consumidores en
O0, O1, O2 y O3. En cada optimización ejecutó TLS externa compartida y TLS
interna independiente, ambas con el control en el hilo principal y con otro
hilo real. Las 16 ejecuciones terminaron correctamente, sin los encabezados
originales de los productores, usando LLD y el arranque TLS existente.

El gate de identidad invoca ahora el validador agrupado en lugar de repetir sus
cuatro controles solo en O0. No se ha ejecutado la batería completa de la tanda
8; tampoco se certifican DLL ni otros tipos TLS mediante esta muestra.
