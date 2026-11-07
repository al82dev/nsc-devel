# Alcance inicial de IR atómico

Se preparan rutas LLVM para lectura, escritura y suma/resta de un global C
atómico `int` representado por el indicador MIR, sin activar su proyección.
No se usa una secuencia separada de carga y escritura para actualizaciones.
