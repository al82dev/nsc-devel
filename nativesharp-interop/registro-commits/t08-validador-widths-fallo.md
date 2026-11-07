# T08: propagación del fallo en punteros atómicos

Una copia aislada del validador widths devolvió cero después de fallar la
emisión dentro del bucle. Se envían las fallas de generación IR y emisión
a un destino que presenta los registros y devuelve uno. Pendiente de
comprobación con una nueva falla inducida y las rutas normales global/TLS.
