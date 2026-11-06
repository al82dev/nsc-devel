# Controles de procedencia del buffer Clang

La suite verifica que dos records directos compartan la huella del buffer
virtual `probe.c` con offsets distintos y que un record generado por macro no
reclame ubicación directa. No acredita un archivo físico ni identidad nominal.
La primera construcción detectó una variable local `pair` duplicada dentro de
la suite. Se renombraron las variables del control; la construcción incremental
y la ejecución de la suite terminaron con código 0. El caso compartido y la
validación completa de la tanda siguen pendientes.
