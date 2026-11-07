# Proyección inicial de TLS C atómico int

El inventario admite `_Thread_local _Atomic(int)` con tipo y alineación de 32
bits comprobados, conserva los calificadores y marca el almacenamiento como
atómico. Otras formas siguen rechazadas. El soporte depende del descenso y de
las barreras preparadas, y queda sujeto a la prueba positiva y regresiones.
