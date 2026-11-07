# Proyección de punteros TLS C anidados y void

El control C pasó y NSC rechazó `int**` y `void*` en la barrera TLS. Se
permite que punteros C con elemento lleguen al mapeo existente, que valida
profundidad, anchura, elementos y calificadores. No se afirma soporte de todas
las formas; construcción incremental y validación agrupada siguen pendientes.
