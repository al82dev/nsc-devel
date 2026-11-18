# Controles de argumentos nombrados en inicializadores

Los inicializadores `this(...)` y `base(...)` comparten la explicación de rechazo de
ownership con la construcción directa. Se agregaron dos controles con argumentos
nombrados reordenados para comprobar que el mensaje identifica el argumento 2 y el
parámetro `first`, cuya transferencia falta.

El compilador de la copia rechazó ambas reproducciones con el mensaje esperado. El
blanco incremental `nsc_project_construction_model_tests` pasó 84 comprobaciones.
Este commit solo amplía pruebas; no cambia código de producto.
