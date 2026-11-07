# T08: propagación de errores en la matriz atómica

Los validadores de la matriz atómica comparten el patrón que devolvió cero
ante un fallo dentro del bucle de emisión. Las ramas con registro pasan a un
destino de fallo explícito que muestra los logs y devuelve uno, incluyendo
los controles negativos que deben fallar. Pendiente de validar el conjunto y
provocar fallos en copias aisladas.
