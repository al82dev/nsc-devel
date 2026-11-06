# Envoltura preparada de procedencia C

Se añade un codec interno versionado que encapsula el catálogo C existente y
añade la procedencia opcional de cada record. Comprueba bandera, contador, hash,
estructura y codificación canónica; no altera la lectura o escritura NSMETA
1.42 ni habilita la nueva envoltura en bibliotecas. Pendiente de controles y
validación agrupada. Una huella coincidente no prueba identidad nominal.
