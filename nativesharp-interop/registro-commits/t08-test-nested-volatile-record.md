# Struct volatile con campos anidados

El fixture prepara un campo struct anidado y un campo puntero en un global
volatile. C contrasta los valores; el IR debe conservar volatile en el campo
anidado y no transferirlo al objeto apuntado. La dirección sin calificador del
campo anidado debe rechazarse. Pendiente de validación agrupada.
