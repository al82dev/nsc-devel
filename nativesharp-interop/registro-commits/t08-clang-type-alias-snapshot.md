# Tanda 08: captura de aliases de Clang

La sesión copia typedefs y aliases nombrados no dependientes de la unidad,
namespaces y bloques de linkage, con tipo subyacente y ubicación. Evita duplicar
redeclaraciones canónicas y no conserva punteros al AST. Los aliases anidados en
records y su resolución semántica desde NativeSharp aún no se implementan.
Pendiente de la validación agrupada de la tanda.
