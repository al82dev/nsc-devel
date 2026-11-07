# Proyección de punteros TLS a escalares C

El control C de los punteros y calificadores pasó; NSC los rechazó en la
proyección TLS. Se admite el puntero a elemento escalar C, dejando al mapeo
existente validar sus tipos, anchura y calificadores, y preservando el
tratamiento `const`/`volatile` del puntero y del destino. Pendiente la
construcción incremental y la validación agrupada estática y DLL. No se
habilitan punteros a records, funciones o arreglos por este cambio.
