# Clase prestada mediante una vista de parámetro

Una vista local obtenida de un parámetro `Rc` por valor permitía leer
`view.Get()` directamente, pero rechazaba guardar el resultado de clase en un
local `borrowed`, aun dentro de la vida del dueño y de la vista. Se observó el
diagnóstico de origen no válido antes del cambio, sin conservar un log separado
de esa primera ejecución.

El análisis reconoce ahora la raíz de una vista local cuando resuelve a un
parámetro move-only por valor. El préstamo conserva el ordinal del parámetro y
termina como máximo al salir del ámbito de la vista. El cambio se limita a esa
forma y no establece garantías para todos los receptores, alias de vista,
parámetros prestados, metadata o consumo binario.

Tras una compilación incremental, los controles internos local y atómico y
las pruebas públicas desde fuentes ejecutaron la lectura O0/O2 con salida 42
y rechazaron mover al dueño mientras el alias estaba vivo. Para `Rc` y `Arc`
se rechazaron además el reemplazo del dueño, la escritura por el alias y su
retorno; el reemplazo tras terminar los ámbitos ejecutó con 42 en O0/O2.
