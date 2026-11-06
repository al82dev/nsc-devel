# Emisión volatile para globales enteros

T08-015: las lecturas directas emiten acquire y las escrituras release cuando
el dato de MIR demuestra alineación, el tipo entero coincide con su tamaño y
Clang habilitó MS volatile. Una asignación compuesta conserva cargas y
escrituras separadas; no se presenta como operación RMW indivisible. Otros
accesos conservan la emisión anterior. Falta la validación agrupada.
