# T08-004: identidad entre sesiones de global C estatica

`pruebas-tanda-08-global-static/validar.cmd` paso en O0–O3 con dos productores
precompilados y un consumidor directo. Cada modulo conservo su instancia y
observo los mismos datos mediante acceso directo y funciones inline. El control
con dos encabezados que declaran el mismo nombre se rechazo con diagnostico
de origenes ambiguos. La construccion incremental se habia completado antes de
esta ejecucion; no se ejecuto la bateria general de la tanda 8.

Quedan otros tipos, calificadores, TLS y otros escenarios de biblioteca; esta
evidencia no cierra T08-004 ni la tanda.
