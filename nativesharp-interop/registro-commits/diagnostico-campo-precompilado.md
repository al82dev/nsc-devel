# Diagnóstico de campo C importado

El consumidor que combina el header C y una biblioteca precompilada recibió
`C++ instance field owner, layout or name is invalid`. La validación reúne
varias condiciones y el mensaje anterior no permite identificar cuál falló.
Se conserva la misma comprobación y se agrega la identidad, el nombre y el
primer motivo rechazado para localizar la causa sin omitir validaciones.
Pendiente de construcción incremental y reproducción.
