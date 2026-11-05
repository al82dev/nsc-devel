# Tanda 08: representación inicial de arrays fijos

`cppAliasArrayType` conserva las dimensiones fijas en orden C exterior a
interior y utiliza el mapeo escalar existente para el elemento. Arreglos sin
límite conocido o con calificadores aún no representados permanecen sin
proyección: no se inventa longitud. Esta función aún no modifica el AST; su
conexión y pruebas forman parte de la tanda sin validar.
