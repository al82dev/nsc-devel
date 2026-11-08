# Identidad de funciones internas de dos bibliotecas

Dos productores importan encabezados `same.h` distintos con una función
`static inline` del mismo nombre. Clang reporta el mismo USR para ambas, pero
sus cuerpos devuelven valores diferentes. Cada productor publica una plantilla;
el consumidor llama ambas y comprueba 11 y 29 respectivamente.

`inline-dos-bibliotecas/validar.cmd` construyó los dos productores y el
consumidor y pasó en O0. La evidencia cubre esa muestra, no todas las formas
de enlace interno ni otras optimizaciones.
