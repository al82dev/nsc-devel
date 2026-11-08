# Captura de carriers anónimos C11

La sesión de Clang conserva si un campo físico es un struct o union anónimo de C11,
separándolo de otros campos sin nombre como los bitfields. La prueba de identidad
incluye ambos carriers y un bitfield sin nombre.

Este cambio solo incorpora la marca a la representación en memoria: no proyecta aún
los miembros promovidos ni los serializa. T08-010 y T08-011 permanecen abiertos;
el consumo directo y el precompilado todavía no quedan habilitados por esta marca.
La construcción y la prueba conjunta de la tanda siguen pendientes.
