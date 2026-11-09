# Préstamo de `Get()` desde un parámetro por valor

Una lectura local `borrowed Payload alias = owner.Get()` era rechazada cuando
`owner` era un `Rc` recibido por valor: el compilador no reconocía el receptor
como dueño de la referencia devuelta. Se observó el diagnóstico de préstamo sin
origen antes del cambio; la salida de esa primera ejecución no se guardó en un
log independiente.

Para una llamada de instancia cuyo resultado `ref` declara vida `this`, el
análisis reconoce ahora un receptor que sea parámetro move-only por valor y
registra su ordinal y el ámbito de la función como límite del préstamo. La
regla no cambia el tratamiento de receptores temporales o parámetros prestados.

Tras compilar incrementalmente, las pruebas dirigidas de los controles interno
local y atómico y de las familias públicas desde fuentes ejecutaron lecturas
en O0/O2 y rechazaron mover o reemplazar al dueño con el alias activo. La
selección pública rechazó también escribir por el alias y devolverlo, y permitió
el reemplazo después de terminar el préstamo, con ejecución 42 en O0/O2. No
se acredita el consumo binario, la concurrencia ni otras formas de receptor.
