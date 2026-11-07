# Control negativo TLS de arreglo

La prueba negativa pasa de `int*` a un arreglo `int[2]` TLS, que sigue fuera
de la proyección. La prueba del puntero se mantiene por separado como caso
positivo estático y DLL. Ambos controles esperan la validación agrupada.
