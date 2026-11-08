# Tanda 8: función C desde plantilla precompilada

El fixture compila una biblioteca C, genera una biblioteca NativeSharp cuya
plantilla llama a esa función, verifica la función en su NSMETA 1.46 y compila,
enlaza y ejecuta un consumidor en otra carpeta sin el encabezado C. Se copian
la biblioteca NativeSharp, su metadata y la biblioteca C necesaria.

La construcción incremental y el fixture terminaron con código 0 en O0; el
fixture previo de alias también pasó en O0. No se probaron O1–O3, DLL, tipos
complejos, conflictos, enlace interno ni la batería completa de la tanda.
