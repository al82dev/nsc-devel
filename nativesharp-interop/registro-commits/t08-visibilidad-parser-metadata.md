# T08: visibilidad de tipos del proveedor sintactico precompilado

La prueba configura el proveedor de tipos precompilados incorporado en `db34a213c` con un tipo visible desde App. Comprueba que el parser interpreta su uso como declaracion de puntero en App y conserva una multiplicacion en Core, que no depende de App. Verifica lo mismo al clonar la sintaxis para observacion.

Se compilo incrementalmente y se ejecuto `nsc_cpp_session_tests` con la cadena de A:; codigo 0. El control usa un proveedor simulado y no sustituye la prueba de catalogo real ni cubre todas las relaciones entre proyectos. Las dependencias C de plantillas y la validacion completa de la tanda 8 siguen pendientes.
