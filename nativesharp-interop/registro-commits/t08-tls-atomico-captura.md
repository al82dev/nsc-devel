# Captura inicial de `_Atomic(T)`

La sesión de Clang distingue el tipo atómico y conserva su elemento como tipo
anidado, además del tamaño y la alineación del objeto. El nuevo valor se añade
después de `Unsupported`, por lo que los codecs previos que limitan el rango a
ese valor continúan rechazándolo. No se habilita acceso ni se afirma soporte.
