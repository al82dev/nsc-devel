# Controles de procedencia del buffer Clang

La suite verifica que dos records directos compartan la huella del buffer
virtual `probe.c` con offsets distintos y que un record generado por macro no
reclame ubicación directa. No acredita un archivo físico ni identidad nominal.
Los controles esperan construcción y validación agrupadas.
