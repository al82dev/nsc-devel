# Lectura y escritura atómicas preparadas

Las rutas LLVM preparadas para el indicador atómico de global C `int` emiten
lectura y escritura `seq_cst` con alineación comprobada; otros tipos o
alineaciones se rechazan. La proyección continúa deshabilitada y falta validar
las operaciones compuestas y los escapes antes de aceptar el global.
