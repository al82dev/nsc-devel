# Alineación preparada para global C atómico

El HIR conserva una alineación de cuatro bytes para un global C atómico `int`
solo cuando el tamaño y la alineación del almacenamiento están demostrados.
La proyección continúa deshabilitada hasta completar la generación y barreras.
