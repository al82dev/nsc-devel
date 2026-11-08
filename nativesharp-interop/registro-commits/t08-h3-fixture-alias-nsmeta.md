# Tanda 8: prueba real de alias C en plantilla precompilada

El fixture compila un productor con un typedef de un encabezado C, inspecciona
que su NSMETA 1.46 contenga ese alias y compila y ejecuta un consumidor desde
otra carpeta a la que se copian únicamente la biblioteca y la metadata. El
consumidor no recibe el encabezado C.

El fixture terminó con código 0 en O0 usando el NSC de desarrollo de A: con la
emisión 1.46 preparada en el mismo árbol de trabajo. Se agregaron después
comprobaciones de error de copia al script, sin volverlo a ejecutar. No cubre
O1–O3, DLL, colisiones, funciones ni globales, y no constituye la validación
completa de la tanda.
