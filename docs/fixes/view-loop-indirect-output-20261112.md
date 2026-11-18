# Salidas de vista indirectas entre iteraciones

En `b92dadbaf`, los ejemplos de
`A:/nsc-fixes-evidencia/view-delegate-backedge-20261112.py` aceptaron sin
diagnóstico dos escapes posibles a través de delegados `ref/out`; el control
con dueños del mismo ámbito fue aceptado. Se observó el mismo resultado para
una invocación de evento (`view-event-backedge-before-20261112.log`). Los
negativos se analizaron, no se ejecutaron.

Las invocaciones de delegados y eventos tienen registros distintos de los de
métodos y funciones globales. La revisión del backedge incorpora ahora sus
bindings de invocación del mismo callable cuando un argumento `ref/out`
identifica un local de vista. Reconstruye los argumentos y sus calificaciones
para emplear el comprobador de salida existente y la unión conservadora en
punto fijo; no usa un nombre desconocido como identidad exacta.

Con el target `nsc` construido incrementalmente, SHA-256
`ce163fd9113c2933c3abc4fd594eaefd848531e9b6a508b09b6ae861b36340fc`,
las pruebas dirigidas `view_delegate_loop_outputs.py` y
`view_event_loop_outputs.py` pasaron seis casos cada una: ref, out, for,
cadenas y dos controles seguros. Los escapes recibieron el diagnóstico del
dueño interior y no se ejecutaron. Registros:
`A:/nsc-fixes-evidencia/view-indirect-delegate-final-20261112.log` y
`A:/nsc-fixes-evidencia/view-indirect-event-expanded-20261112.log`.
Los positivos de cada familia compilaron y ejecutaron en O0/O2 con salidas 7
y 42 (`view-delegate-positive-final-20261112.log` y
`view-event-positive-final-20261112.log`). También pasaron las regresiones
dirigidas de métodos, funciones globales y tuplas.

La comprobación trata únicamente las invocaciones y destinos locales
identificados. Receptores opacos, destinos indirectos o proyectados, otros
tipos que contengan vistas y productores no identificados necesitan análisis
adicional; el resultado no certifica todas las rutas de vida ni concurrencia.
No se ejecutó batería completa ni se modificó interoperabilidad C/C++.
