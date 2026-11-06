# Envoltura preparada de procedencia C

Se añade un codec interno versionado que encapsula el catálogo C existente y
añade la procedencia opcional de cada record. Comprueba bandera, contador, hash,
estructura y codificación canónica; no altera la lectura o escritura NSMETA
1.42 ni habilita la nueva envoltura en bibliotecas. La construcción incremental
y la suite de sesión terminaron con código 0 tras `27f7b2080`. Una huella
coincidente no prueba identidad nominal; la integración sigue pendiente.
