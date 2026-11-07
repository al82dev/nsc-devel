# Proyección de puntero TLS C a entero

El control C reprodujo el funcionamiento del puntero TLS y NSC rechazó su
proyección. `projectGlobalWithLookup` habilita únicamente `int*` mutable a
`int` mutable de 32 bits; el mapeo y la generación de almacenamiento ya
existentes siguen siendo responsables del tipo y de su dirección TLS.
No se amplían punteros con calificadores, otros tipos, arreglos ni TLS C++.
Pendiente construcción incremental y validación agrupada estática y DLL.
