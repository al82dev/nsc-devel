# Proyección de arreglo TLS C de dos enteros

El control C del arreglo pasó y NSC lo rechazó en `projectGlobalWithLookup`.
Se habilita únicamente `int[2]` mutable de enteros de 32 bits, usando el
mapeo de arreglos y las rutas TLS existentes. Arreglos de otros tamaños,
dimensiones, elementos y calificadores permanecen pendientes. La construcción
y las pruebas estática y DLL aún están pendientes.
