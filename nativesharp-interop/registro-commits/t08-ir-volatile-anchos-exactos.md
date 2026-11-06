# Anchura y alineación exactas en IR volatile

El gate anterior exigía acquire/release pero aceptaba cualquier ancho y no
comprobaba el valor de alineación. Ahora exige i8/1, i16/2 e i64/8 para cada
global con y sin signo. Esta precisión se comprobará en la siguiente batería
agrupada; la validación anterior no demuestra los valores exactos.
