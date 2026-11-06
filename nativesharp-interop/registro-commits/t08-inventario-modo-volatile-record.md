# T08-015: modo volatile de la sesión en records

Cada observación de record conserva ahora el valor efectivo reportado por su
sesión Clang. Si no está disponible permanece ausente; no se deduce del triple.
Esto no cambia aún la generación ni resuelve diferencias entre observaciones.
La construcción y validación están pendientes.
