# T08-006: integración de almacenamiento bool en records C

Se integra el detector y la lectura/escritura de byte de `fb5375b35` del carril
Claude. La rama principal ya carga elementos `bool*` como i8 y trunca después;
se conserva esa ruta existente sin aplicar un segundo truncamiento. Las rutas
de globales y almacenamiento atómico se conservan. Falta construcción y
validación en la rama principal; no se acredita el orden de campos volatile.
