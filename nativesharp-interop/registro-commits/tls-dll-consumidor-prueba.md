# Consumidor NativeSharp de DLL con TLS C

La matriz LLD amplía la validación de la DLL: comprueba que la exportación
NativeSharp de lectura existe y compila un consumidor NativeSharp que usa la
biblioteca publicada sin incluir el encabezado C. El consumidor debe leer y
escribir el TLS y verificar el aislamiento entre hilos. La prueba está
preparada y todavía no se ha ejecutado. No modifica NSLink ni los controles
de selección anteriores.
