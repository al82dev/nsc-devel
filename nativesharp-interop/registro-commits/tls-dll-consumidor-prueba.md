# Consumidor NativeSharp de DLL con TLS C

La matriz LLD amplía la validación de la DLL: comprueba que la exportación
NativeSharp de lectura existe y compila un consumidor NativeSharp que usa la
biblioteca publicada sin incluir el encabezado C. El consumidor debe leer y
escribir el TLS y verificar el aislamiento entre hilos. La prueba pasó en O0
con NSC build `b910fa13ea6d5855`. No modifica NSLink ni los controles de
selección anteriores. No demuestra aún la matriz O1–O3 del consumidor DLL.
