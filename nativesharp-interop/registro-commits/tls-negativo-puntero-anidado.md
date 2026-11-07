# Control negativo de puntero TLS anidado

El control negativo usa `int**` TLS, aún no proyectado. Los arreglos
escalares `volatile` se prueban en el fixture positivo separado. Ambos casos
esperan la validación agrupada.
