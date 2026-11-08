# T08: comparaciones directas de procedencia de enums

El control negativo de integración rechazaba `NMood` sin demostrar que el
valor distinto fuera la causa. Este control dirigido compara el enum real de
Clang con su copia restaurada desde metadata y prueba por separado coincidencia,
valor distinto, desplazamiento distinto y ausencia de procedencia o USR.

La prueba de integración conserva su diagnóstico; estas verificaciones no
sustituyen una prueba completa del valor modificado en la ruta del driver.
La construcción y ejecución están pendientes.
